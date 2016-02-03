/*
 * Copyright (c) 2012-2016 The Linux Foundation. All rights reserved.
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


/*========================================================================

  \file  wlan_hdd_main.c

  \brief WLAN Host Device Driver implementation

  ========================================================================*/

/**=========================================================================

                       EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$   $DateTime: $ $Author: $


  when        who    what, where, why
  --------    ---    --------------------------------------------------------
  04/5/09     Shailender     Created module.
  02/24/10    Sudhir.S.Kohalli  Added to support param for SoftAP module
  06/03/10    js - Added support to hostapd driven deauth/disassoc/mic failure
  ==========================================================================*/

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <wlan_hdd_includes.h>
#include <vos_api.h>
#include <vos_sched.h>
#ifdef WLAN_FEATURE_LPSS
#include <vos_utils.h>
#endif
#include <linux/etherdevice.h>
#include <linux/firmware.h>
#include <wcnss_api.h>
#include <wlan_hdd_tx_rx.h>
#include <wniApi.h>
#include <wlan_nlink_srv.h>
#include <wlan_btc_svc.h>
#include <wlan_hdd_cfg.h>
#include <wlan_ptt_sock_svc.h>
#include <dbglog_host.h>
#include <wlan_logging_sock_svc.h>
#include <wlan_hdd_wowl.h>
#include <wlan_hdd_misc.h>
#include <wlan_hdd_wext.h>
#include "wlan_hdd_trace.h"
#include "vos_types.h"
#include "vos_trace.h"

#include<net/addrconf.h>
#include <linux/wireless.h>
#include <net/cfg80211.h>
#include <linux/inetdevice.h>
#include <net/addrconf.h>
#include "wlan_hdd_cfg80211.h"
#include "wlan_hdd_p2p.h"
#include <linux/rtnetlink.h>
#include "sapApi.h"
#include <linux/semaphore.h>
#include <linux/ctype.h>
#include <linux/compat.h>
#ifdef MSM_PLATFORM
#ifdef CONFIG_CNSS
#include <soc/qcom/subsystem_restart.h>
#endif
#endif
#include <wlan_hdd_hostapd.h>
#include <wlan_hdd_softap_tx_rx.h>
#include "cfgApi.h"
#include "wlan_hdd_dev_pwr.h"
#include "qwlan_version.h"
#include "wlan_qct_wda.h"
#include "wlan_hdd_tdls.h"
#ifdef FEATURE_WLAN_CH_AVOID
#include "vos_cnss.h"
#include "regdomain_common.h"

extern int hdd_hostapd_stop (struct net_device *dev);
void hdd_ch_avoid_cb(void *hdd_context,void *indi_param);
#endif /* FEATURE_WLAN_CH_AVOID */

#ifdef WLAN_FEATURE_NAN
#include "wlan_hdd_nan.h"
#endif /* WLAN_FEATURE_NAN */

#include "wlan_hdd_debugfs.h"
#include "epping_main.h"
#include "wlan_hdd_memdump.h"

#include <wlan_hdd_ipa.h>
#if defined(HIF_PCI)
#include "if_pci.h"
#elif defined(HIF_USB)
#include "if_usb.h"
#elif defined(HIF_SDIO)
#include "if_ath_sdio.h"
#endif
#include "wma.h"
#include "ol_fw.h"
#include "wlan_hdd_ocb.h"
#include "wlan_hdd_tsf.h"

#if defined(LINUX_QCMBR)
#define SIOCIOCTLTX99 (SIOCDEVPRIVATE+13)
#endif

#ifdef QCA_ARP_SPOOFING_WAR
#include "ol_if_athvar.h"
#define HDD_ARP_PACKET_TYPE_OFFSET 12
#endif

#ifdef MODULE
#define WLAN_MODULE_NAME  module_name(THIS_MODULE)
#else
#define WLAN_MODULE_NAME  "wlan"
#endif

#ifdef TIMER_MANAGER
#define TIMER_MANAGER_STR " +TIMER_MANAGER"
#else
#define TIMER_MANAGER_STR ""
#endif

#ifdef MEMORY_DEBUG
#define MEMORY_DEBUG_STR " +MEMORY_DEBUG"
#else
#define MEMORY_DEBUG_STR ""
#endif

#define DISABLE_KRAIT_IDLE_PS_VAL   1
#ifdef IPA_UC_OFFLOAD
/* If IPA UC data path is enabled, target should reserve extra tx descriptors
 * for IPA WDI data path.
 * Then host data path should allow less TX packet pumping in case
 * IPA WDI data path enabled */
#define WLAN_TFC_IPAUC_TX_DESC_RESERVE   100
#endif /* IPA_UC_OFFLOAD */

/* the Android framework expects this param even though we don't use it */
#define BUF_LEN 20
static char fwpath_buffer[BUF_LEN];
static struct kparam_string fwpath = {
   .string = fwpath_buffer,
   .maxlen = BUF_LEN,
};

static char *country_code;
static int   enable_11d = -1;
static int   enable_dfs_chan_scan = -1;

#ifndef MODULE
static int wlan_hdd_inited;
#endif

/*
 * spinlock for synchronizing asynchronous request/response
 * (full description of use in wlan_hdd_main.h)
 */
DEFINE_SPINLOCK(hdd_context_lock);

/*
 * The rate at which the driver sends RESTART event to supplicant
 * once the function 'vos_wlanRestart()' is called
 *
 */
#define WLAN_HDD_RESTART_RETRY_DELAY_MS 5000  /* 5 second */
#define WLAN_HDD_RESTART_RETRY_MAX_CNT  5     /* 5 retries */

/*
 * Size of Driver command strings from upper layer
 */
#define SIZE_OF_SETROAMMODE             11    /* size of SETROAMMODE */
#define SIZE_OF_GETROAMMODE             11    /* size of GETROAMMODE */

/*
 * Ibss prop IE from command will be of size:
 * size  = sizeof(oui) + sizeof(oui_data) + 1(Element ID) + 1(EID Length)
 * OUI_DATA should be at least 3 bytes long
 */
#define WLAN_HDD_IBSS_MIN_OUI_DATA_LENGTH (3)


#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
#define TID_MIN_VALUE 0
#define TID_MAX_VALUE 15
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */
/*
 * Maximum buffer size used for returning the data back to user space
 */
#define WLAN_MAX_BUF_SIZE 1024
#define WLAN_PRIV_DATA_MAX_LEN    8192
/*
 * Driver miracast parameters 0-Disabled
 * 1-Source, 2-Sink
 */
#define WLAN_HDD_DRIVER_MIRACAST_CFG_MIN_VAL 0
#define WLAN_HDD_DRIVER_MIRACAST_CFG_MAX_VAL 2

/*
 * When ever we need to print IBSSPEERINFOALL for more than 16 STA
 * we will split the printing.
 */
#define NUM_OF_STA_DATA_TO_PRINT 16

#define WLAN_NLINK_CESIUM 30

/*
 * Android DRIVER command structures
 */
struct android_wifi_reassoc_params {
   unsigned char bssid[18];
   int channel;
};

#define ANDROID_WIFI_ACTION_FRAME_SIZE 1040
struct android_wifi_af_params {
   unsigned char bssid[18];
   int channel;
   int dwell_time;
   int len;
   unsigned char data[ANDROID_WIFI_ACTION_FRAME_SIZE];
} ;

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
#define WLAN_HDD_MAX_TCP_PORT            65535
#define WLAN_WAIT_TIME_READY_TO_EXTWOW   2000
#endif

#define AUTO_SUSPEND_DELAY_MS    1500

static vos_wake_lock_t wlan_wake_lock;
/* set when SSR is needed after unload */
static e_hdd_ssr_required isSsrRequired = HDD_SSR_NOT_REQUIRED;

#define WOW_MAX_FILTER_LISTS     1
#define WOW_MAX_FILTERS_PER_LIST 4
#define WOW_MIN_PATTERN_SIZE     6
#define WOW_MAX_PATTERN_SIZE     64

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0)) || defined(WITH_BACKPORTS)
static const struct wiphy_wowlan_support wowlan_support_reg_init = {
    .flags = WIPHY_WOWLAN_ANY |
             WIPHY_WOWLAN_MAGIC_PKT |
             WIPHY_WOWLAN_DISCONNECT |
             WIPHY_WOWLAN_SUPPORTS_GTK_REKEY |
             WIPHY_WOWLAN_GTK_REKEY_FAILURE |
             WIPHY_WOWLAN_EAP_IDENTITY_REQ |
             WIPHY_WOWLAN_4WAY_HANDSHAKE |
             WIPHY_WOWLAN_RFKILL_RELEASE,
    .n_patterns = WOW_MAX_FILTER_LISTS * WOW_MAX_FILTERS_PER_LIST,
    .pattern_min_len = WOW_MIN_PATTERN_SIZE,
    .pattern_max_len = WOW_MAX_PATTERN_SIZE,
};
#endif

/* Internal function declarations */
static int hdd_driver_init(void);
static void hdd_driver_exit(void);

/* Internal function declarations */

static void hdd_tx_fail_ind_callback(v_U8_t *MacAddr, v_U8_t seqNo);

static struct sock *cesium_nl_srv_sock;
static v_U16_t cesium_pid;

static int hdd_ParseIBSSTXFailEventParams(tANI_U8 *pValue,
                                          tANI_U8 *tx_fail_count,
                                          tANI_U16 *pid);

void wlan_hdd_restart_timer_cb(v_PVOID_t usrDataForCallback);

struct completion wlan_start_comp;
#ifdef QCA_WIFI_FTM
extern int hdd_ftm_start(hdd_context_t *pHddCtx);
extern int hdd_ftm_stop(hdd_context_t *pHddCtx);
#endif
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
v_VOID_t wlan_hdd_auto_shutdown_cb(v_VOID_t);
#endif

/* Store WLAN driver version info in a global variable such that crash debugger
   can extract it from driver debug symbol and crashdump for post processing */
tANI_U8 g_wlan_driver_version[ ] = QWLAN_VERSIONSTR;

/**
 * hdd_device_mode_to_string() - return string conversion of device mode
 * @device_mode: device mode
 *
 * This utility function helps log string conversion of device mode.
 *
 * Return: string conversion of device mode, if match found;
 *	   "Unknown" otherwise.
 */
const char* hdd_device_mode_to_string(uint8_t device_mode)
{
	switch (device_mode) {
	CASE_RETURN_STRING(WLAN_HDD_INFRA_STATION);
	CASE_RETURN_STRING(WLAN_HDD_SOFTAP);
	CASE_RETURN_STRING(WLAN_HDD_P2P_CLIENT);
	CASE_RETURN_STRING(WLAN_HDD_P2P_GO);
	CASE_RETURN_STRING(WLAN_HDD_MONITOR);
	CASE_RETURN_STRING(WLAN_HDD_FTM);
	CASE_RETURN_STRING(WLAN_HDD_IBSS);
	CASE_RETURN_STRING(WLAN_HDD_P2P_DEVICE);
	CASE_RETURN_STRING(WLAN_HDD_OCB);
	default:
		return "Unknown";
	}
}

#ifdef QCA_LL_TX_FLOW_CT

/**
 * wlan_hdd_clean_tx_flow_control_timer - Function cleans tx flow control timer
 * @hddctx: pointer to hddctx
 * @hdd_adapter_t: pointer to hdd_adapter_t
 *
 * Function deregister's, destroy tx flow control timer
 *
 * Return: None
 */
void wlan_hdd_clean_tx_flow_control_timer(hdd_context_t *hddctx,
					hdd_adapter_t *adapter)
{
	WLANTL_DeRegisterTXFlowControl(hddctx->pvosContext,
					adapter->sessionId);
	if (adapter->tx_flow_timer_initialized == VOS_TRUE) {
		vos_timer_destroy(&adapter->tx_flow_control_timer);
		adapter->tx_flow_timer_initialized = VOS_FALSE;
	}
}

#endif

/**
 * wlan_hdd_find_opclass() - Find operating class for a channel
 * @hal: handler to HAL
 * @channel: channel id
 * @bw_offset: bandwidth offset
 *
 * Function invokes sme api to find the operating class
 *
 * Return: operating class
 */
uint8_t wlan_hdd_find_opclass(tHalHandle hal, uint8_t channel,
	                      uint8_t bw_offset)
{
	uint8_t opclass = 0;

	sme_get_opclass(hal, channel, bw_offset, &opclass);
	return opclass;
}

#ifdef CONFIG_CNSS
/**
 * hdd_request_pm_qos() - vote performance management qos
 * @val: Memory latency usec requirement to ensure
 *
 * To endure CPU hardware wake up, vote to performance management module.
 * Current voting will prevent hardware power collapse then can reduce CPU
 * wake up latency.
 * Performance QoS voting will ensure WLAN performance.
 * Start up time and throughput
 *
 * Return: none
 */
static inline void hdd_request_pm_qos(int val)
{
	cnss_request_pm_qos(val);
}

/**
 * hdd_remove_pm_qos() - Cancel performance voting
 *
 * Not need to stay wake up CPU any more. Remove voting to release CPU
 *
 * Return: none
 */
static inline void hdd_remove_pm_qos(void)
{
	cnss_remove_pm_qos();
}
#else
/**
 * hdd_request_pm_qos() - vote performance management qos
 * @val: Memory latency usec requirement to ensure
 *
 * To endure CPU hardware wake up, vote to performance management module.
 * Current voting will prevent hardware power collapse then can reduce CPU
 * wake up latency.
 * Performance QoS votign will ensure WLAN performance.
 * Start up time and throughput
 *
 * Return: none
 */
static inline void hdd_request_pm_qos(int val)
{
}

/**
 * hdd_remove_pm_qos() - Cancel performance voting
 *
 * Not need to stay wake up CPU any more. Remove voting to release CPU
 *
 * Return: none
 */
static inline void hdd_remove_pm_qos(void)
{
}
#endif

#ifdef FEATURE_GREEN_AP

static void hdd_wlan_green_ap_timer_fn(void *phddctx)
{
    hdd_context_t *pHddCtx = (hdd_context_t *)phddctx;
    hdd_green_ap_ctx_t *green_ap;

    if (0 != wlan_hdd_validate_context(pHddCtx))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return;
    }
    green_ap = pHddCtx->green_ap_ctx;

    if (green_ap)
        hdd_wlan_green_ap_mc(pHddCtx, green_ap->ps_event);
}

static VOS_STATUS hdd_wlan_green_ap_attach(hdd_context_t *pHddCtx)
{
    hdd_green_ap_ctx_t *green_ap;
    VOS_STATUS status = VOS_STATUS_SUCCESS;

    ENTER();

    green_ap = vos_mem_malloc(sizeof(hdd_green_ap_ctx_t));

    if (!green_ap) {
        hddLog(LOGP, FL("Memory allocation for Green-AP failed!"));
        status = VOS_STATUS_E_NOMEM;
        goto error;
    }

    vos_mem_zero((void *)green_ap, sizeof(*green_ap));
    green_ap->pHddContext = pHddCtx;
    pHddCtx->green_ap_ctx = green_ap;

    green_ap->ps_state = GREEN_AP_PS_OFF_STATE;
    green_ap->ps_event = 0;
    green_ap->num_nodes = 0;
    green_ap->ps_on_time = GREEN_AP_PS_ON_TIME;
    green_ap->ps_delay_time = GREEN_AP_PS_DELAY_TIME;

    vos_timer_init(&green_ap->ps_timer,
            VOS_TIMER_TYPE_SW,
            hdd_wlan_green_ap_timer_fn,
            (void *)pHddCtx);

error:

    EXIT();
    return status;
}

static VOS_STATUS hdd_wlan_green_ap_deattach(hdd_context_t *pHddCtx)
{
    hdd_green_ap_ctx_t *green_ap = pHddCtx->green_ap_ctx;
    VOS_STATUS status = VOS_STATUS_SUCCESS;

    ENTER();

    if (green_ap == NULL) {
        hddLog(LOG1, FL("Green-AP is not enabled"));
        status = VOS_STATUS_E_NOSUPPORT;
        goto done;
    }

    /* check if the timer status is destroyed */
    if (VOS_TIMER_STATE_RUNNING ==
            vos_timer_getCurrentState(&green_ap->ps_timer))
    {
        vos_timer_stop(&green_ap->ps_timer);
    }

    /* Destroy the Green AP timer */
    if (!VOS_IS_STATUS_SUCCESS(vos_timer_destroy(
                    &green_ap->ps_timer)))
    {
        hddLog(LOG1, FL("Cannot deallocate Green-AP's timer"));
    }

    /* release memory */
    vos_mem_zero((void *)green_ap, sizeof(*green_ap));
    vos_mem_free(green_ap);
    pHddCtx->green_ap_ctx = NULL;

done:

    EXIT();
    return status;
}

static void hdd_wlan_green_ap_update(hdd_context_t *pHddCtx,
    hdd_green_ap_ps_state_t state,
    hdd_green_ap_event_t event)
{
    hdd_green_ap_ctx_t *green_ap = pHddCtx->green_ap_ctx;

    green_ap->ps_state = state;
    green_ap->ps_event = event;
}

static int hdd_wlan_green_ap_enable(hdd_adapter_t *pHostapdAdapter,
        v_U8_t enable)
{
    int ret = 0;

    hddLog(LOG1, "%s: Set Green-AP val: %d", __func__, enable);

    ret = process_wma_set_command(
            (int)pHostapdAdapter->sessionId,
            (int)WMI_PDEV_GREEN_AP_PS_ENABLE_CMDID,
            enable,
            DBG_CMD);

    return ret;
}

void hdd_wlan_green_ap_mc(hdd_context_t *pHddCtx,
        hdd_green_ap_event_t event)
{
    hdd_green_ap_ctx_t *green_ap = pHddCtx->green_ap_ctx;
    hdd_adapter_t *pAdapter = NULL;

    if (green_ap == NULL)
        return ;

    hddLog(LOG1, "%s: Green-AP event: %d, state: %d, num_nodes: %d",
        __func__, event, green_ap->ps_state, green_ap->num_nodes);

    /* handle the green ap ps event */
    switch(event) {
        case GREEN_AP_PS_START_EVENT:
            green_ap->ps_enable = 1;
            break;

        case GREEN_AP_PS_STOP_EVENT:
            if (!(hdd_get_concurrency_mode() & VOS_SAP))
                green_ap->ps_enable = 0;
            break;

        case GREEN_AP_ADD_STA_EVENT:
            green_ap->num_nodes++;
            break;

        case GREEN_AP_DEL_STA_EVENT:
            if (green_ap->num_nodes)
                green_ap->num_nodes--;
            break;

        case GREEN_AP_PS_ON_EVENT:
        case GREEN_AP_PS_WAIT_EVENT:
            break;

        default:
            hddLog(LOGE, "%s: invalid event %d", __func__, event);
            break;
    }

    /* Confirm that power save is enabled  before doing state transitions */
    if (!green_ap->ps_enable) {
        hddLog(VOS_TRACE_LEVEL_INFO, FL("Green-AP is disabled"));
        hdd_wlan_green_ap_update(pHddCtx,
            GREEN_AP_PS_IDLE_STATE, GREEN_AP_PS_WAIT_EVENT);
        goto done;
    }

    pAdapter = hdd_get_adapter (pHddCtx, WLAN_HDD_SOFTAP );

    if (pAdapter == NULL) {
        hddLog(LOGE, FL("Green-AP no SAP adapter"));
        goto done;
    }

    /* handle the green ap ps state */
    switch(green_ap->ps_state) {
        case GREEN_AP_PS_IDLE_STATE:
            hdd_wlan_green_ap_update(pHddCtx,
                GREEN_AP_PS_OFF_STATE, GREEN_AP_PS_WAIT_EVENT);
            break;

        case GREEN_AP_PS_OFF_STATE:
            if (!green_ap->num_nodes) {
                hdd_wlan_green_ap_update(pHddCtx,
                    GREEN_AP_PS_WAIT_STATE, GREEN_AP_PS_WAIT_EVENT);
                vos_timer_start(&green_ap->ps_timer,
                        green_ap->ps_delay_time);
            }
            break;

        case GREEN_AP_PS_WAIT_STATE:
            if (!green_ap->num_nodes) {
                hdd_wlan_green_ap_update(pHddCtx,
                   GREEN_AP_PS_ON_STATE, GREEN_AP_PS_WAIT_EVENT);

                hdd_wlan_green_ap_enable(pAdapter, 1);

                if (green_ap->ps_on_time) {
                    hdd_wlan_green_ap_update(pHddCtx,
                        0, GREEN_AP_PS_WAIT_EVENT);
                    vos_timer_start(&green_ap->ps_timer,
                            green_ap->ps_on_time);
                }
            } else {
                hdd_wlan_green_ap_update(pHddCtx,
                    GREEN_AP_PS_OFF_STATE, GREEN_AP_PS_WAIT_EVENT);
            }
            break;

        case GREEN_AP_PS_ON_STATE:
            if (green_ap->num_nodes) {
                if (hdd_wlan_green_ap_enable(pAdapter, 0)) {
                    hddLog(LOGE, FL("FAILED TO SET GREEN-AP mode"));
                    goto done;
                }
                hdd_wlan_green_ap_update(pHddCtx,
                    GREEN_AP_PS_OFF_STATE, GREEN_AP_PS_WAIT_EVENT);
            } else if ((green_ap->ps_event == GREEN_AP_PS_WAIT_EVENT) &&
                    (green_ap->ps_on_time)) {

                /* ps_on_time timeout, switch to ps off */
                hdd_wlan_green_ap_update(pHddCtx,
                    GREEN_AP_PS_WAIT_STATE, GREEN_AP_PS_ON_EVENT);

                if (hdd_wlan_green_ap_enable(pAdapter, 0)) {
                    hddLog(LOGE, FL("FAILED TO SET GREEN-AP mode"));
                    goto done;
                }

                vos_timer_start(&green_ap->ps_timer,
                        green_ap->ps_delay_time);
            }
            break;

        default:
            hddLog(LOGE, "%s: invalid state %d", __func__, green_ap->ps_state);
            hdd_wlan_green_ap_update(pHddCtx,
                GREEN_AP_PS_OFF_STATE, GREEN_AP_PS_WAIT_EVENT);
            break;
    }

done:
    return;
}

/**
 * hdd_wlan_green_ap_init() - Initialize Green AP feature
 * @hdd_ctx: HDD global context
 *
 * Return: none
 */
void hdd_wlan_green_ap_init(struct hdd_context_s *hdd_ctx)
{
	if (!VOS_IS_STATUS_SUCCESS(hdd_wlan_green_ap_attach(hdd_ctx)))
		hddLog(LOGE, FL("Failed to allocate Green-AP resource"));
}

/**
 * hdd_wlan_green_ap_deinit() - De-initialize Green AP feature
 * @hdd_ctx: HDD global context
 *
 * Return: none
 */
void hdd_wlan_green_ap_deinit(struct hdd_context_s *hdd_ctx)
{
	if (!VOS_IS_STATUS_SUCCESS(hdd_wlan_green_ap_deattach(hdd_ctx)))
		hddLog(LOGE, FL("Cannot deallocate Green-AP resource"));
}

/**
 * wlan_hdd_set_egap_support() - helper function to set egap support flag
 * @hdd_ctx:   pointer to hdd context
 * @cfg:       pointer to hdd target configuration knob
 *
 * Return:     None
 */
void wlan_hdd_set_egap_support(hdd_context_t *hdd_ctx, struct hdd_tgt_cfg *cfg)
{
	if (hdd_ctx && cfg)
		hdd_ctx->green_ap_ctx->egap_support = cfg->egap_support;
}

/**
 * hdd_wlan_green_ap_start_bss() - Notify Green AP of Start BSS event
 * @hdd_ctx: HDD global context
 *
 * Return: none
 */
void hdd_wlan_green_ap_start_bss(struct hdd_context_s *hdd_ctx)
{
	hdd_config_t *cfg;

	if (!hdd_ctx) {
		hddLog(LOGE, FL("hdd context is NULL"));
		goto exit;
	}

	cfg = hdd_ctx->cfg_ini;

	if (!cfg) {
		hddLog(LOGE, FL("hdd cfg is NULL"));
		goto exit;
	}

	/* check if the firmware and ini are both enabled the egap,
	 * and also the feature_flag enable, then we enable the egap
	 */
	if (hdd_ctx->green_ap_ctx->egap_support && cfg->enable_egap &&
	    cfg->egap_feature_flag) {
		hddLog(LOG1,
		       FL("Set EGAP - enabled: %d, flag: %x, inact_time: %d, wait_time: %d"),
			  cfg->enable_egap,
			  cfg->egap_feature_flag,
			  cfg->egap_inact_time,
			  cfg->egap_wait_time);
		if (!sme_send_egap_conf_params(cfg->enable_egap,
					       cfg->egap_inact_time,
					       cfg->egap_wait_time,
					       cfg->egap_feature_flag)) {
			/* EGAP is enabled, disable host GAP */
			hdd_wlan_green_ap_mc(hdd_ctx, GREEN_AP_PS_STOP_EVENT);
			goto exit;
		}
		/* fall through, if send_egap_conf_params() failed,
		 * then check host GAP and enable it accordingly
		 */
	}

	if (!(VOS_STA & hdd_ctx->concurrency_mode) &&
	    cfg->enable2x2 && cfg->enableGreenAP) {
		hdd_wlan_green_ap_mc(hdd_ctx, GREEN_AP_PS_START_EVENT);
	} else {
		hdd_wlan_green_ap_mc(hdd_ctx, GREEN_AP_PS_STOP_EVENT);
		hddLog(LOG1,
		       FL("Green AP disabled - sta_con: %d, 2x2: %d, GAP: %d"),
		       (VOS_STA & hdd_ctx->concurrency_mode),
		       cfg->enable2x2, cfg->enableGreenAP);
	}
exit:
	return;
}

/**
 * hdd_wlan_green_ap_stop_bss() - Notify Green AP of Stop BSS event
 * @hdd_ctx: HDD global context
 *
 * Return: none
 */
void hdd_wlan_green_ap_stop_bss(struct hdd_context_s *hdd_ctx)
{
	hdd_wlan_green_ap_mc(hdd_ctx, GREEN_AP_PS_STOP_EVENT);
}

/**
 * hdd_wlan_green_ap_add_sta() - Notify Green AP of Add Station  event
 * @hdd_ctx: HDD global context
 *
 * Return: none
 */
void hdd_wlan_green_ap_add_sta(struct hdd_context_s *hdd_ctx)
{
	hdd_wlan_green_ap_mc(hdd_ctx, GREEN_AP_ADD_STA_EVENT);
}

/**
 * hdd_wlan_green_ap_del_sta() - Notify Green AP of Delete Station event
 * @hdd_ctx: HDD global context
 *
 * Return: none
 */
void hdd_wlan_green_ap_del_sta(struct hdd_context_s *hdd_ctx)
{
	hdd_wlan_green_ap_mc(hdd_ctx, GREEN_AP_DEL_STA_EVENT);
}

#endif /* FEATURE_GREEN_AP */

/**
 * hdd_lost_link_info_cb() - callback function to get lost link information
 * @context: HDD context
 * @lost_link_info: lost link information
 *
 * Return: none
 */
static void hdd_lost_link_info_cb(void *context,
				  struct sir_lost_link_info *lost_link_info)
{
	hdd_context_t *hdd_ctx = (hdd_context_t *)context;
	int status;
	hdd_adapter_t *adapter;

	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status) {
		hddLog(LOGE, "%s: HDD context is not valid", __func__);
		return;
	}

	if (NULL == lost_link_info) {
		hddLog(LOGE, "%s: lost_link_info is NULL", __func__);
		return;
	}

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, lost_link_info->vdev_id);
	if (NULL == adapter) {
		hddLog(LOGE, "%s: invalid adapter", __func__);
		return;
	}

	adapter->rssi_on_disconnect = lost_link_info->rssi;
	hddLog(LOG1, "%s: rssi on disconnect %d",
		     __func__, adapter->rssi_on_disconnect);
}

/**
 * __hdd_smps_force_mode_cb() - callback for smps force mode
 * event
 * @context: HDD context
 * @smps_mode_event: smps force mode event info
 *
 * Return: none
 */
static void __hdd_smps_force_mode_cb(void *context,
		struct sir_smps_force_mode_event *smps_mode_event)
{
	hdd_context_t *hdd_ctx = (hdd_context_t *)context;
	int status;
	hdd_adapter_t *adapter;

	ENTER();

	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status)
		return;

	if (NULL == smps_mode_event) {
		hddLog(LOGE, FL("smps_mode_event is NULL"));
		return;
	}

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, smps_mode_event->vdev_id);
	if (NULL == adapter) {
		hddLog(LOGE, FL("Invalid adapter"));
		return;
	}

	adapter->smps_force_mode_status = smps_mode_event->status;

	complete(&adapter->smps_force_mode_comp_var);

	hddLog(LOG1, FL("status %d vdev_id: %d"),
	       smps_mode_event->status, smps_mode_event->vdev_id);
}

/**
 * hdd_smps_force_mode_cb() - Wrapper to protect
 * __hdd_smps_force_mode_cb callback for smps force mode event
 * @context: HDD context
 * @smps_mode_event: smps force mode event info
 *
 * Return: none
 */
static void hdd_smps_force_mode_cb(void *context,
		struct sir_smps_force_mode_event *smps_mode_event)
{
	vos_ssr_protect(__func__);
	__hdd_smps_force_mode_cb(context, smps_mode_event);
	vos_ssr_unprotect(__func__);

}

#if defined (FEATURE_WLAN_MCC_TO_SCC_SWITCH) || defined (FEATURE_WLAN_STA_AP_MODE_DFS_DISABLE)
/**---------------------------------------------------------------------------

  \brief wlan_hdd_restart_sap() - This function is used to restart SAP in
                                  driver internally

  \param  - ap_adapter - Pointer to SAP hdd_adapter_t structure

  \return - void

  --------------------------------------------------------------------------*/

static void wlan_hdd_restart_sap(hdd_adapter_t *ap_adapter)
{
    hdd_ap_ctx_t *pHddApCtx;
    hdd_hostapd_state_t *pHostapdState;
    VOS_STATUS vos_status;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(ap_adapter);
#ifdef CFG80211_DEL_STA_V2
    struct station_del_parameters delStaParams;
#endif
    tsap_Config_t *pConfig;

    pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
    pConfig = &ap_adapter->sessionCtx.ap.sapConfig;

    mutex_lock(&pHddCtx->sap_lock);
    if (test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags)) {
#ifdef CFG80211_DEL_STA_V2
        delStaParams.mac = NULL;
        delStaParams.subtype = SIR_MAC_MGMT_DEAUTH >> 4;
        delStaParams.reason_code = eCsrForcedDeauthSta;
        wlan_hdd_cfg80211_del_station(ap_adapter->wdev.wiphy, ap_adapter->dev,
                                      &delStaParams);
#else
        wlan_hdd_cfg80211_del_station(ap_adapter->wdev.wiphy, ap_adapter->dev,
                                      NULL);
#endif
        hdd_cleanup_actionframe(pHddCtx, ap_adapter);

        pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(ap_adapter);
        vos_event_reset(&pHostapdState->stop_bss_event);

        if ( VOS_STATUS_SUCCESS == WLANSAP_StopBss(
#ifdef WLAN_FEATURE_MBSSID
            pHddApCtx->sapContext
#else
            (WLAN_HDD_GET_CTX(ap_adapter))->pvosContext
#endif
        )) {
            vos_status = vos_wait_single_event(&pHostapdState->stop_bss_event,
                                               10000);
            if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
                hddLog(LOGE, FL("SAP Stop Failed"));
                goto end;
            }
        }
        clear_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags);
        wlan_hdd_decr_active_session(pHddCtx, ap_adapter->device_mode);
        hddLog(LOGE, FL("SAP Stop Success"));

        if (0 != wlan_hdd_cfg80211_update_apies(ap_adapter)) {
            hddLog(LOGE, FL("SAP Not able to set AP IEs"));
            WLANSAP_ResetSapConfigAddIE(pConfig, eUPDATE_IE_ALL);
            goto end;
        }

        if (WLANSAP_StartBss(
#ifdef WLAN_FEATURE_MBSSID
            pHddApCtx->sapContext,
#else
            pHddCtx->pvosContext,
#endif
            hdd_hostapd_SAPEventCB, &pHddApCtx->sapConfig,
            (v_PVOID_t)ap_adapter->dev) != VOS_STATUS_SUCCESS) {
            hddLog(LOGE, FL("SAP Start Bss fail"));
            goto end;
        }

        hddLog(LOG1, FL("Waiting for SAP to start"));
        vos_status = vos_wait_single_event(&pHostapdState->vosEvent, 10000);
        if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
            hddLog(LOGE, FL("SAP Start failed"));
            goto end;
        }
        hddLog(LOGE, FL("SAP Start Success"));
        set_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags);
        wlan_hdd_incr_active_session(pHddCtx, ap_adapter->device_mode);
        pHostapdState->bCommit = TRUE;
    }
end:
    mutex_unlock(&pHddCtx->sap_lock);
    return;
}
#endif

static int __hdd_netdev_notifier_call(struct notifier_block * nb,
                                         unsigned long state,
                                         void *data)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0))
   struct netdev_notifier_info *dev_notif_info = data;
   struct net_device *dev = dev_notif_info->dev;
#else
   struct net_device *dev = data;
#endif
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_context_t *pHddCtx;

   //Make sure that this callback corresponds to our device.
   if ((strncmp(dev->name, "wlan", 4)) &&
      (strncmp(dev->name, "p2p", 3)))
      return NOTIFY_DONE;

   if ((pAdapter->magic != WLAN_HDD_ADAPTER_MAGIC) ||
      (pAdapter->dev != dev)) {
      hddLog(LOGE, FL("device adapter is not matching!!!"));
      return NOTIFY_DONE;
   }

   if (!dev->ieee80211_ptr) {
      hddLog(LOGE, FL("ieee80211_ptr is NULL!!!"));
      return NOTIFY_DONE;
   }

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   if (NULL == pHddCtx)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD Context Null Pointer", __func__);
      VOS_ASSERT(0);
      return NOTIFY_DONE;
   }
   if (pHddCtx->isLogpInProgress)
      return NOTIFY_DONE;


   hddLog(VOS_TRACE_LEVEL_INFO, "%s: %s New Net Device State = %lu",
          __func__, dev->name, state);

   switch (state) {
   case NETDEV_REGISTER:
        break;

   case NETDEV_UNREGISTER:
        break;

   case NETDEV_UP:
#ifdef FEATURE_WLAN_CH_AVOID
        sme_ChAvoidUpdateReq(pHddCtx->hHal);
#endif
        break;

   case NETDEV_DOWN:
        break;

   case NETDEV_CHANGE:
        if(TRUE == pAdapter->isLinkUpSvcNeeded)
           complete(&pAdapter->linkup_event_var);
        break;

   case NETDEV_GOING_DOWN:
        if( pAdapter->scan_info.mScanPending != FALSE )
        {
           unsigned long rc;
           INIT_COMPLETION(pAdapter->scan_info.abortscan_event_var);
           hdd_abort_mac_scan(pAdapter->pHddCtx, pAdapter->sessionId,
                              eCSR_SCAN_ABORT_DEFAULT);
           rc = wait_for_completion_timeout(
                               &pAdapter->scan_info.abortscan_event_var,
                               msecs_to_jiffies(WLAN_WAIT_TIME_ABORTSCAN));
           if (!rc) {
              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                        "%s: Timeout occurred while waiting for abortscan",
                        __func__);
           }
        }
        else
        {
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
               "%s: Scan is not Pending from user" , __func__);
        }
        break;

   default:
        break;
   }

   return NOTIFY_DONE;
}

/**
 * hdd_netdev_notifier_call() - netdev notifier callback function
 * @nb: pointer to notifier block
 * @state: state
 * @ndev: ndev pointer
 *
 * Return: 0 on success, error number otherwise.
 */
static int hdd_netdev_notifier_call(struct notifier_block * nb,
					unsigned long state,
					void *ndev)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __hdd_netdev_notifier_call(nb, state, ndev);
	vos_ssr_unprotect(__func__);

	return ret;
}

struct notifier_block hdd_netdev_notifier = {
   .notifier_call = hdd_netdev_notifier_call,
};

/*---------------------------------------------------------------------------
 *   Function definitions
 *-------------------------------------------------------------------------*/
void hdd_unregister_mcast_bcast_filter(hdd_context_t *pHddCtx);
void hdd_register_mcast_bcast_filter(hdd_context_t *pHddCtx);
//variable to hold the insmod parameters
static int con_mode;
#ifndef MODULE
/* current con_mode - used only for statically linked driver
 * con_mode is changed by user space to indicate a mode change which will
 * result in calling the module exit and init functions. The module
 * exit function will clean up based on the value of con_mode prior to it
 * being changed by user space. So curr_con_mode records the current con_mode
 * for exit when con_mode becomes the next mode for init
 */
static int curr_con_mode;
#endif

/**---------------------------------------------------------------------------

  \brief hdd_vos_trace_enable() - Configure initial VOS Trace enable

  Called immediately after the cfg.ini is read in order to configure
  the desired trace levels.

  \param  - moduleId - module whose trace level is being configured
  \param  - bitmask - bitmask of log levels to be enabled

  \return - void

  --------------------------------------------------------------------------*/
static void hdd_vos_trace_enable(VOS_MODULE_ID moduleId, v_U32_t bitmask)
{
   VOS_TRACE_LEVEL level;

   /* if the bitmask is the default value, then a bitmask was not
      specified in cfg.ini, so leave the logging level alone (it
      will remain at the "compiled in" default value) */
   if (CFG_VOS_TRACE_ENABLE_DEFAULT == bitmask)
   {
      return;
   }

   /* a mask was specified.  start by disabling all logging */
   vos_trace_setValue(moduleId, VOS_TRACE_LEVEL_NONE, 0);

   /* now cycle through the bitmask until all "set" bits are serviced */
   level = VOS_TRACE_LEVEL_FATAL;
   while (0 != bitmask)
   {
      if (bitmask & 1)
      {
         vos_trace_setValue(moduleId, level, 1);
      }
      level++;
      bitmask >>= 1;
   }
}


/*
 * FUNCTION: wlan_hdd_validate_context
 * This function is used to check the HDD context
 */
int wlan_hdd_validate_context(hdd_context_t *pHddCtx)
{

    if (NULL == pHddCtx || NULL == pHddCtx->cfg_ini) {
        hddLog(LOG1, FL("HDD context is Null"));
        return -ENODEV;
    }

    if (pHddCtx->isLogpInProgress) {
        hddLog(LOG1, FL("LOGP in Progress. Ignore!!!"));
        return -EAGAIN;
    }

    if ((pHddCtx->isLoadInProgress) ||
        (pHddCtx->isUnloadInProgress)) {
        hddLog(LOG1, FL("Unloading/Loading in Progress. Ignore!!!"));
        return -EAGAIN;
    }
    return 0;
}


void hdd_checkandupdate_phymode( hdd_context_t *pHddCtx)
{
   hdd_adapter_t *pAdapter = NULL;
   hdd_station_ctx_t *pHddStaCtx = NULL;
   eCsrPhyMode phyMode;
   hdd_config_t *cfg_param = NULL;

   if (NULL == pHddCtx)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
               "HDD Context is null !!");
       return ;
   }

   pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_INFRA_STATION);
   if (NULL == pAdapter)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
               "pAdapter is null !!");
       return ;
   }

   pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

   cfg_param = pHddCtx->cfg_ini;
   if (NULL == cfg_param)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
               "cfg_params not available !!");
       return ;
   }

   phyMode = sme_GetPhyMode(WLAN_HDD_GET_HAL_CTX(pAdapter));

   if (!pHddCtx->isVHT80Allowed)
   {
       if ((eCSR_DOT11_MODE_AUTO == phyMode) ||
           (eCSR_DOT11_MODE_11ac == phyMode) ||
           (eCSR_DOT11_MODE_11ac_ONLY == phyMode))
       {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                    "Setting phymode to 11n!!");
           sme_SetPhyMode(WLAN_HDD_GET_HAL_CTX(pAdapter), eCSR_DOT11_MODE_11n);
       }
   }
   else
   {
       /*New country Supports 11ac as well resetting value back from .ini*/
       sme_SetPhyMode(WLAN_HDD_GET_HAL_CTX(pAdapter),
             hdd_cfg_xlate_to_csr_phy_mode(cfg_param->dot11Mode));
       return ;
   }

   if ((eConnectionState_Associated == pHddStaCtx->conn_info.connState) &&
       ((eCSR_CFG_DOT11_MODE_11AC_ONLY == pHddStaCtx->conn_info.dot11Mode) ||
        (eCSR_CFG_DOT11_MODE_11AC == pHddStaCtx->conn_info.dot11Mode)))
   {
       VOS_STATUS vosStatus;

       // need to issue a disconnect to CSR.
       INIT_COMPLETION(pAdapter->disconnect_comp_var);
       vosStatus = sme_RoamDisconnect(WLAN_HDD_GET_HAL_CTX(pAdapter),
                          pAdapter->sessionId,
                          eCSR_DISCONNECT_REASON_UNSPECIFIED );

       if (VOS_STATUS_SUCCESS == vosStatus)
       {
           unsigned long rc;

           rc = wait_for_completion_timeout(&pAdapter->disconnect_comp_var,
                      msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
           if (!rc)
               hddLog(LOGE, FL("failure waiting for disconnect_comp_var"));
        }
   }
}

static void
hdd_checkandupdate_dfssetting(hdd_adapter_t *pAdapter, char *country_code)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    hdd_config_t *cfg_param;

    if (NULL == pHddCtx)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                "HDD Context is null !!");
        return ;
    }

    cfg_param = pHddCtx->cfg_ini;

    if (NULL == cfg_param)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                "cfg_params not available !!");
        return ;
    }

    if (NULL != strstr(cfg_param->listOfNonDfsCountryCode, country_code))
    {
       /*New country doesn't support DFS */
       sme_UpdateDfsSetting(WLAN_HDD_GET_HAL_CTX(pAdapter), 0);
    }
    else
    {
       /*New country Supports DFS as well resetting value back from .ini*/
       sme_UpdateDfsSetting(WLAN_HDD_GET_HAL_CTX(pAdapter), cfg_param->enableDFSChnlScan);
    }

}

/* Function header is left blank intentionally */
static int hdd_parse_setrmcenable_command(tANI_U8 *pValue, tANI_U8 *pRmcEnable)
{
    tANI_U8 *inPtr = pValue;
    int tempInt;
    int v = 0;
    char buf[32];
    *pRmcEnable = 0;

    inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);

    if (NULL == inPtr)
    {
        return 0;
    }

    else if (SPACE_ASCII_VALUE != *inPtr)
    {
        return 0;
    }

    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr)) inPtr++;

    if ('\0' == *inPtr)
    {
        return 0;
    }

    sscanf(inPtr, "%32s ", buf);
    v = kstrtos32(buf, 10, &tempInt);
    if ( v < 0)
    {
       return -EINVAL;
    }

    *pRmcEnable = tempInt;

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
       "ucRmcEnable: %d", *pRmcEnable);

    return 0;
}

/* Function header is left blank intentionally */
static int hdd_parse_setrmcactionperiod_command(tANI_U8 *pValue,
           tANI_U32 *pActionPeriod)
{
    tANI_U8 *inPtr = pValue;
    int tempInt;
    int v = 0;
    char buf[32];
    *pActionPeriod = 0;

    inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);

    if (NULL == inPtr)
    {
        return -EINVAL;
    }

    else if (SPACE_ASCII_VALUE != *inPtr)
    {
        return -EINVAL;
    }

    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr)) inPtr++;

    if ('\0' == *inPtr)
    {
        return 0;
    }

    sscanf(inPtr, "%32s ", buf);
    v = kstrtos32(buf, 10, &tempInt);
    if ( v < 0)
    {
       return -EINVAL;
    }

    if ( (tempInt < WNI_CFG_RMC_ACTION_PERIOD_FREQUENCY_STAMIN) ||
         (tempInt > WNI_CFG_RMC_ACTION_PERIOD_FREQUENCY_STAMAX) )
    {
       return -EINVAL;
    }

    *pActionPeriod = tempInt;

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
       "uActionPeriod: %d", *pActionPeriod);

    return 0;
}

/* Function header is left blank intentionally */
static int hdd_parse_setrmcrate_command(tANI_U8 *pValue,
           tANI_U32 *pRate, tTxrateinfoflags *pTxFlags)
{
    tANI_U8 *inPtr = pValue;
    int tempInt;
    int v = 0;
    char buf[32];
    *pRate = 0;
    *pTxFlags = 0;

    inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);

    if (NULL == inPtr)
    {
        return -EINVAL;
    }

    else if (SPACE_ASCII_VALUE != *inPtr)
    {
        return -EINVAL;
    }

    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr)) inPtr++;

    if ('\0' == *inPtr)
    {
        return 0;
    }

    sscanf(inPtr, "%32s ", buf);
    v = kstrtos32(buf, 10, &tempInt);
    if ( v < 0)
    {
       return -EINVAL;
    }

    switch (tempInt)
    {
        default:
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
            "Unsupported rate: %d", tempInt);
            return -EINVAL;
        case 0:
        case 6:
        case 9:
        case 12:
        case 18:
        case 24:
        case 36:
        case 48:
        case 54:
            *pTxFlags = eHAL_TX_RATE_LEGACY;
            *pRate = tempInt * 10;
            break;
        case 65:
            *pTxFlags = eHAL_TX_RATE_HT20;
            *pRate = tempInt * 10;
            break;
        case 72:
            *pTxFlags = eHAL_TX_RATE_HT20 | eHAL_TX_RATE_SGI;
            *pRate = 722;
            break;
    }

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
       "Rate: %d", *pRate);

    return 0;
}


/**
 * hdd_get_ibss_peer_info_cb() - IBSS peer Info request callback
 * @UserData: Adapter private data
 * @pPeerInfoRsp: Peer info response
 *
 * This is an asynchronous callback function from SME when the peer info
 * is received
 *
 * Return: 0 for success non-zero for failure
 */
void
hdd_get_ibss_peer_info_cb(v_VOID_t *pUserData,
                          tSirPeerInfoRspParams *pPeerInfo)
{
   hdd_adapter_t *pAdapter = (hdd_adapter_t *)pUserData;
   hdd_station_ctx_t *pStaCtx;
   v_U8_t   i;

   /*Sanity check*/
   if ((NULL == pAdapter) || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)) {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
         "invalid adapter or adapter has invalid magic");
      return;
   }

   pStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
   if (NULL != pPeerInfo && eHAL_STATUS_SUCCESS == pPeerInfo->status) {
      /* validate number of peers */
      if (pPeerInfo->numPeers < HDD_MAX_NUM_IBSS_STA) {
         pStaCtx->ibss_peer_info.status = pPeerInfo->status;
         pStaCtx->ibss_peer_info.numPeers = pPeerInfo->numPeers;

         for (i = 0; i < pPeerInfo->numPeers; i++) {
             pStaCtx->ibss_peer_info.peerInfoParams[i] =
                                         pPeerInfo->peerInfoParams[i];
         }
         hddLog(LOG1, FL("Peer Info copied in HDD"));
      } else {
         hddLog(LOG1,
                FL("Number of peers %d returned is more than limit %d"),
                pPeerInfo->numPeers, HDD_MAX_NUM_IBSS_STA);
      }
   } else {
      hddLog(LOG1, FL("peerInfo returned is NULL"));
   }

   complete(&pAdapter->ibss_peer_info_comp);
}

/**---------------------------------------------------------------------------

  \brief hdd_cfg80211_get_ibss_peer_info_all() -

  Request function to get IBSS peer info from lower layers

  \pAdapter -> Adapter context

  \return - 0 for success non-zero for failure
  --------------------------------------------------------------------------*/
static
VOS_STATUS hdd_cfg80211_get_ibss_peer_info_all(hdd_adapter_t *pAdapter)
{
   tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
   VOS_STATUS retStatus = VOS_STATUS_E_FAILURE;
   unsigned long rc;

   INIT_COMPLETION(pAdapter->ibss_peer_info_comp);

   retStatus = sme_RequestIBSSPeerInfo(hHal, pAdapter,
                                    hdd_get_ibss_peer_info_cb,
                                    VOS_TRUE, 0xFF);

   if (VOS_STATUS_SUCCESS == retStatus)
   {
      rc = wait_for_completion_timeout
               (&pAdapter->ibss_peer_info_comp,
                msecs_to_jiffies(IBSS_PEER_INFO_REQ_TIMOEUT));

      /* status will be 0 if timed out */
      if (!rc) {
          hddLog(VOS_TRACE_LEVEL_WARN, "%s: Warning: IBSS_PEER_INFO_TIMEOUT",
                 __func__);
          retStatus = VOS_STATUS_E_FAILURE;
          return retStatus;
      }
   }
   else
   {
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: Warning: sme_RequestIBSSPeerInfo Request failed", __func__);
   }

   return retStatus;
}

/**---------------------------------------------------------------------------

  \brief hdd_cfg80211_get_ibss_peer_info() -

  Request function to get IBSS peer info from lower layers

  \pAdapter -> Adapter context
  \staIdx -> Sta index for which the peer info is requested

  \return - 0 for success non-zero for failure
  --------------------------------------------------------------------------*/
static VOS_STATUS
hdd_cfg80211_get_ibss_peer_info(hdd_adapter_t *pAdapter, v_U8_t staIdx)
{
    unsigned long rc;
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    VOS_STATUS retStatus = VOS_STATUS_E_FAILURE;

    INIT_COMPLETION(pAdapter->ibss_peer_info_comp);

    retStatus = sme_RequestIBSSPeerInfo(hHal, pAdapter,
                                     hdd_get_ibss_peer_info_cb,
                                     VOS_FALSE, staIdx);

    if (VOS_STATUS_SUCCESS == retStatus)
    {
       rc = wait_for_completion_timeout
                (&pAdapter->ibss_peer_info_comp,
                msecs_to_jiffies(IBSS_PEER_INFO_REQ_TIMOEUT));

       /* status = 0 on timeout */
       if (!rc) {
          hddLog(VOS_TRACE_LEVEL_WARN, "%s: Warning: IBSS_PEER_INFO_TIMEOUT",
                 __func__);
          retStatus = VOS_STATUS_E_FAILURE;
          return retStatus;
       }
    }
    else
    {
       hddLog(VOS_TRACE_LEVEL_WARN,
              "%s: Warning: sme_RequestIBSSPeerInfo Request failed", __func__);
    }

    return retStatus;
}

/* Function header is left blank intentionally */
static VOS_STATUS
hdd_parse_get_ibss_peer_info(tANI_U8 *pValue, v_MACADDR_t *pPeerMacAddr)
{
    tANI_U8 *inPtr = pValue;
    inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);

    if (NULL == inPtr)
    {
        return VOS_STATUS_E_FAILURE;;
    }

    else if (SPACE_ASCII_VALUE != *inPtr)
    {
        return VOS_STATUS_E_FAILURE;;
    }

    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    if ('\0' == *inPtr)
    {
        return VOS_STATUS_E_FAILURE;;
    }

    if (inPtr[2] != ':' || inPtr[5] != ':' || inPtr[8] != ':' ||
        inPtr[11] != ':' || inPtr[14] != ':')
    {
       return VOS_STATUS_E_FAILURE;;
    }
    sscanf(inPtr, "%2x:%2x:%2x:%2x:%2x:%2x",
                  (unsigned int *)&pPeerMacAddr->bytes[0],
                  (unsigned int *)&pPeerMacAddr->bytes[1],
                  (unsigned int *)&pPeerMacAddr->bytes[2],
                  (unsigned int *)&pPeerMacAddr->bytes[3],
                  (unsigned int *)&pPeerMacAddr->bytes[4],
                  (unsigned int *)&pPeerMacAddr->bytes[5]);

    return VOS_STATUS_SUCCESS;
}

#ifdef IPA_UC_STA_OFFLOAD
static void hdd_set_thermal_level_cb(hdd_context_t *pHddCtx, u_int8_t level)
{
   /* Change IPA to SW path when throttle level greater than 0 */
   if (level > THROTTLE_LEVEL_0)
      hdd_ipa_send_mcc_scc_msg(pHddCtx, TRUE);
   else
      /* restore original concurrency mode */
      hdd_ipa_send_mcc_scc_msg(pHddCtx, pHddCtx->mcc_mode);
}
#else
static void hdd_set_thermal_level_cb(hdd_context_t *pHddCtx, u_int8_t level)
{
}
#endif

/**---------------------------------------------------------------------------

  \brief hdd_setIbssPowerSaveParams - update IBSS Power Save params to WMA.

  This function sets the IBSS power save config parameters to WMA
  which will send it to firmware if FW supports IBSS power save
  before vdev start.

  \param  - hdd_adapter_t Hdd adapter.

  \return - VOS_STATUS VOS_STATUS_SUCCESS on Success and VOS_STATUS_E_FAILURE
            on failure.

  --------------------------------------------------------------------------*/
VOS_STATUS hdd_setIbssPowerSaveParams(hdd_adapter_t *pAdapter)
{
    int ret;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX( pAdapter );

    if (pHddCtx == NULL) {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is null", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    ret = process_wma_set_command(pAdapter->sessionId,
                             WMA_VDEV_IBSS_SET_ATIM_WINDOW_SIZE,
                             pHddCtx->cfg_ini->ibssATIMWinSize,
                             VDEV_CMD);
    if (0 != ret) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("WMA_VDEV_IBSS_SET_ATIM_WINDOW_SIZE failed %d"),
                   ret);
        return VOS_STATUS_E_FAILURE;
    }

    ret = process_wma_set_command(pAdapter->sessionId,
                             WMA_VDEV_IBSS_SET_POWER_SAVE_ALLOWED,
                             pHddCtx->cfg_ini->isIbssPowerSaveAllowed,
                             VDEV_CMD);
    if (0 != ret) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("WMA_VDEV_IBSS_SET_POWER_SAVE_ALLOWED failed %d"),
                   ret);
        return VOS_STATUS_E_FAILURE;
    }

    ret = process_wma_set_command(pAdapter->sessionId,
                             WMA_VDEV_IBSS_SET_POWER_COLLAPSE_ALLOWED,
                             pHddCtx->cfg_ini->isIbssPowerCollapseAllowed,
                             VDEV_CMD);
    if (0 != ret) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("WMA_VDEV_IBSS_SET_POWER_COLLAPSE_ALLOWED failed %d"),
                   ret);
        return VOS_STATUS_E_FAILURE;
    }

    ret = process_wma_set_command(pAdapter->sessionId,
                             WMA_VDEV_IBSS_SET_AWAKE_ON_TX_RX,
                             pHddCtx->cfg_ini->isIbssAwakeOnTxRx,
                             VDEV_CMD);
    if (0 != ret) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("WMA_VDEV_IBSS_SET_AWAKE_ON_TX_RX failed %d"),
                   ret);
        return VOS_STATUS_E_FAILURE;
    }

    ret = process_wma_set_command(pAdapter->sessionId,
                             WMA_VDEV_IBSS_SET_INACTIVITY_TIME,
                             pHddCtx->cfg_ini->ibssInactivityCount,
                             VDEV_CMD);
    if (0 != ret) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("WMA_VDEV_IBSS_SET_INACTIVITY_TIME failed %d"),
                   ret);
        return VOS_STATUS_E_FAILURE;
    }

    ret = process_wma_set_command(pAdapter->sessionId,
                             WMA_VDEV_IBSS_SET_TXSP_END_INACTIVITY_TIME,
                             pHddCtx->cfg_ini->ibssTxSpEndInactivityTime,
                             VDEV_CMD);
    if (0 != ret) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("WMA_VDEV_IBSS_SET_TXSP_END_INACTIVITY_TIME failed %d"),
                   ret);
        return VOS_STATUS_E_FAILURE;
    }

    ret = process_wma_set_command(pAdapter->sessionId,
                             WMA_VDEV_IBSS_PS_SET_WARMUP_TIME_SECS,
                             pHddCtx->cfg_ini->ibssPsWarmupTime,
                             VDEV_CMD);
    if (0 != ret) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("WMA_VDEV_IBSS_PS_SET_WARMUP_TIME_SECS failed %d"),
                   ret);
        return VOS_STATUS_E_FAILURE;
    }

    ret = process_wma_set_command(pAdapter->sessionId,
                            WMA_VDEV_IBSS_PS_SET_1RX_CHAIN_IN_ATIM_WINDOW,
                            pHddCtx->cfg_ini->ibssPs1RxChainInAtimEnable,
                            VDEV_CMD);
    if (0 != ret) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("WMA_VDEV_IBSS_PS_SET_1RX_CHAIN_IN_ATIM_WINDOW failed %d"),
                   ret);
        return VOS_STATUS_E_FAILURE;
    }

    return VOS_STATUS_SUCCESS;
}

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
/**
 * hdd_parse_reassoc_command_data() - HDD Parse reassoc command data
 * @pValue:     Pointer to input data (its a NUL terminated string)
 * @pTargetApBssid: Pointer to target Ap bssid
 * @pChannel:     Pointer to the Target AP channel
 *
 * This function parses the reasoc command data passed in the format
 * REASSOC<space><bssid><space><channel>
 *
 * Return: 0 for success non-zero for failure
 */
static int hdd_parse_reassoc_command_v1_data(const tANI_U8 *pValue,
                                             tANI_U8 *pTargetApBssid,
                                             tANI_U8 *pChannel)
{
    const tANI_U8 *inPtr = pValue;
    int tempInt;
    int v = 0;
    tANI_U8 tempBuf[32];
    /* 12 hexa decimal digits, 5 ':' and '\0' */
    tANI_U8 macAddress[18];


    inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
    /* no argument after the command */
    if (NULL == inPtr) {
        return -EINVAL;
    } else if (SPACE_ASCII_VALUE != *inPtr) {
        /*no space after the command*/
        return -EINVAL;
    }

    /* Removing empty spaces */
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /* no argument followed by spaces */
    if ('\0' == *inPtr) {
        return -EINVAL;
    }

    v = sscanf(inPtr, "%17s", macAddress);
    if (!((1 == v) && hdd_is_valid_mac_address(macAddress))) {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "Invalid MAC address or All hex inputs are not read (%d)", v);
        return -EINVAL;
    }

    pTargetApBssid[0] = hdd_parse_hex(macAddress[0]) << 4 |
                        hdd_parse_hex(macAddress[1]);
    pTargetApBssid[1] = hdd_parse_hex(macAddress[3]) << 4 |
                        hdd_parse_hex(macAddress[4]);
    pTargetApBssid[2] = hdd_parse_hex(macAddress[6]) << 4 |
                        hdd_parse_hex(macAddress[7]);
    pTargetApBssid[3] = hdd_parse_hex(macAddress[9]) << 4 |
                        hdd_parse_hex(macAddress[10]);
    pTargetApBssid[4] = hdd_parse_hex(macAddress[12]) << 4 |
                        hdd_parse_hex(macAddress[13]);
    pTargetApBssid[5] = hdd_parse_hex(macAddress[15]) << 4 |
                        hdd_parse_hex(macAddress[16]);

    /* point to the next argument */
    inPtr = strnchr(inPtr, strlen(inPtr), SPACE_ASCII_VALUE);
    /* no argument after the command */
    if (NULL == inPtr) return -EINVAL;

    /* Removing empty spaces */
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr) {
        return -EINVAL;
    }

    /*getting the next argument ie the channel number */
    v = sscanf(inPtr, "%31s ", tempBuf);
    if (1 != v) return -EINVAL;

    /*
     * Sigma DUT sends connected bssid and channel 0 to indicate
     * driver to issue reassoc to same AP.
     * Hence do not treat channel 0 as invalid.
     */
    v = kstrtos32(tempBuf, 10, &tempInt);
    if ((v < 0) ||
        (tempInt < 0) ||
        (tempInt > WNI_CFG_CURRENT_CHANNEL_STAMAX)) {
        return -EINVAL;
    }

    *pChannel = tempInt;
    return VOS_STATUS_SUCCESS;
}

/*
  \brief hdd_reassoc() - perform a user space-directed reassoc

  \param - pAdapter - Adapter upon which the command was received
  \param - bssid - BSSID with which to reassociate
  \param - channel - channel upon which to reassociate

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
static int
hdd_reassoc(hdd_adapter_t *pAdapter, const tANI_U8 *bssid, const tANI_U8 channel)
{
   hdd_station_ctx_t *pHddStaCtx;
   int ret = 0;

   pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

   /* if not associated, no need to proceed with reassoc */
   if (eConnectionState_Associated != pHddStaCtx->conn_info.connState) {
      hddLog(VOS_TRACE_LEVEL_INFO, "%s: Not associated", __func__);
      ret = -EINVAL;
      goto exit;
   }

   /* if the target bssid is same as currently associated AP,
      then no need to proceed with reassoc */
   if (!memcmp(bssid, pHddStaCtx->conn_info.bssId, sizeof(tSirMacAddr))) {
      hddLog(VOS_TRACE_LEVEL_INFO,
             "%s: Reassoc BSSID is same as currently associated AP bssid",
             __func__);
      ret = -EINVAL;
      goto exit;
   }

   /* Check channel number is a valid channel number */
   if (VOS_STATUS_SUCCESS !=
       wlan_hdd_validate_operation_channel(pAdapter, channel)) {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid Channel %d",
             __func__, channel);
      ret = -EINVAL;
      goto exit;
   }

   /* Proceed with reassoc */
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
   {
      tCsrHandoffRequest handoffInfo;
      hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

      handoffInfo.channel = channel;
      handoffInfo.src = REASSOC;
      memcpy(handoffInfo.bssid, bssid, sizeof(tSirMacAddr));
      sme_HandoffRequest(pHddCtx->hHal, pAdapter->sessionId, &handoffInfo);
   }
#endif
 exit:
   return ret;
}

/*
  \brief hdd_parse_reassoc_v1() - parse version 1 of the REASSOC command

  This function parses the v1 REASSOC command with the format

      REASSOC xx:xx:xx:xx:xx:xx CH

  Where "xx:xx:xx:xx:xx:xx" is the Hex-ASCII representation of the
  BSSID and CH is the ASCII representation of the channel.  For
  example

      REASSOC 00:0a:0b:11:22:33 48

  \param - pAdapter - Adapter upon which the command was received
  \param - command - ASCII text command that was received

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
static int
hdd_parse_reassoc_v1(hdd_adapter_t *pAdapter, const char *command)
{
   tANI_U8 channel = 0;
   tSirMacAddr bssid;
   int ret;

   ret = hdd_parse_reassoc_command_v1_data(command, bssid, &channel);
   if (ret)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Failed to parse reassoc command data", __func__);
   } else {
      ret = hdd_reassoc(pAdapter, bssid, channel);
   }
   return ret;
}

/*
  \brief hdd_parse_reassoc_v2() - parse version 2 of the REASSOC command

  This function parses the v2 REASSOC command with the format

      REASSOC <android_wifi_reassoc_params>

  \param - pAdapter - Adapter upon which the command was received
  \param - command - command that was received, ASCII command followed
                     by binary data

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
static int
hdd_parse_reassoc_v2(hdd_adapter_t *pAdapter,
                     const char *command)
{
   struct android_wifi_reassoc_params params;
   tSirMacAddr bssid;
   int ret;

   /* The params are located after "REASSOC " */
   memcpy(&params, command + 8, sizeof(params));

   if (!mac_pton(params.bssid, (u8 *)&bssid)) {
      hddLog(LOGE, "%s: MAC address parsing failed", __func__);
      ret = -EINVAL;
   } else {
      ret = hdd_reassoc(pAdapter, bssid, params.channel);
   }
   return ret;
}

/*
  \brief hdd_parse_reassoc() - parse the REASSOC command

  There are two different versions of the REASSOC command.  Version 1
  of the command contains a parameter list that is ASCII characters
  whereas version 2 contains a combination of ASCII and binary
  payload.  Determine if a version 1 or a version 2 command is being
  parsed by examining the parameters, and then dispatch the parser
  that is appropriate for the command.

  \param - pAdapter - Adapter upon which the command was received
  \param - command - command that was received

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
static int
hdd_parse_reassoc(hdd_adapter_t *pAdapter, const char *command)
{
   int ret;

   /* both versions start with "REASSOC "
    * v1 has a bssid and channel # as an ASCII string
    *    REASSOC xx:xx:xx:xx:xx:xx CH
    * v2 has a C struct
    *    REASSOC <binary c struct>
    *
    * The first field in the v2 struct is also the bssid in ASCII.
    * But in the case of a v2 message the BSSID is NUL-terminated.
    * Hence we can peek at that offset to see if this is V1 or V2
    * REASSOC xx:xx:xx:xx:xx:xx*
    *           1111111111222222
    * 01234567890123456789012345
    */
   if (command[25]) {
      ret = hdd_parse_reassoc_v1(pAdapter, command);
   } else {
      ret = hdd_parse_reassoc_v2(pAdapter, command);
   }

   return ret;
}

/**
 * hdd_parse_send_action_frame_v1_data() - HDD Parse send action frame data
 * @pValue:     Pointer to input data (its a NUL terminated string)
 * @pTargetApBssid: Pointer to target Ap bssid
 * @pChannel:       Pointer to the Target AP channel
 * @pDwellTime:     pDwellTime Pointer to the time to stay off-channel
 *                  after transmitting action frame
 * @pBuf:           Pointer to data
 * @pBufLen:        Pointer to data length
 *
 * This function parses the send action frame data passed in the format
 * SENDACTIONFRAME<space><bssid><space><channel><space><dwelltime><space><data>
 *
 * Return: 0 for success non-zero for failure
 */
static int
hdd_parse_send_action_frame_v1_data(const tANI_U8 *pValue,
                                    tANI_U8 *pTargetApBssid,
                                    tANI_U8 *pChannel, tANI_U8 *pDwellTime,
                                    tANI_U8 **pBuf, tANI_U8 *pBufLen)
{
    const tANI_U8 *inPtr = pValue;
    const tANI_U8 *dataEnd;
    int tempInt;
    int j = 0;
    int i = 0;
    int v = 0;
    tANI_U8 tempBuf[32];
    tANI_U8 tempByte = 0;
    /* 12 hexa decimal digits, 5 ':' and '\0' */
    tANI_U8 macAddress[18];

    inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
    /* no argument after the command */
    if (NULL == inPtr) {
        return -EINVAL;
    } else if (SPACE_ASCII_VALUE != *inPtr) {
        /* no space after the command */
        return -EINVAL;
    }

    /* removing empty spaces */
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr) {
        return -EINVAL;
    }

    v = sscanf(inPtr, "%17s", macAddress);
    if (!((1 == v) && hdd_is_valid_mac_address(macAddress))) {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "Invalid MAC address or All hex inputs are not read (%d)", v);
        return -EINVAL;
    }

    pTargetApBssid[0] = hdd_parse_hex(macAddress[0]) << 4 |
                        hdd_parse_hex(macAddress[1]);
    pTargetApBssid[1] = hdd_parse_hex(macAddress[3]) << 4 |
                        hdd_parse_hex(macAddress[4]);
    pTargetApBssid[2] = hdd_parse_hex(macAddress[6]) << 4 |
                        hdd_parse_hex(macAddress[7]);
    pTargetApBssid[3] = hdd_parse_hex(macAddress[9]) << 4 |
                        hdd_parse_hex(macAddress[10]);
    pTargetApBssid[4] = hdd_parse_hex(macAddress[12]) << 4 |
                        hdd_parse_hex(macAddress[13]);
    pTargetApBssid[5] = hdd_parse_hex(macAddress[15]) << 4 |
                        hdd_parse_hex(macAddress[16]);

    /* point to the next argument */
    inPtr = strnchr(inPtr, strlen(inPtr), SPACE_ASCII_VALUE);
    /* no argument after the command */
    if (NULL == inPtr) return -EINVAL;

    /* removing empty spaces */
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /* no argument followed by spaces */
    if ('\0' == *inPtr) {
        return -EINVAL;
    }

    /* getting the next argument ie the channel number */
    v = sscanf(inPtr, "%31s ", tempBuf);
    if (1 != v) return -EINVAL;

    v = kstrtos32(tempBuf, 10, &tempInt);
    if (v < 0 || tempInt < 0 || tempInt > WNI_CFG_CURRENT_CHANNEL_STAMAX)
     return -EINVAL;

    *pChannel = tempInt;

    /* point to the next argument */
    inPtr = strnchr(inPtr, strlen(inPtr), SPACE_ASCII_VALUE);
    /* no argument after the command */
    if (NULL == inPtr) return -EINVAL;
    /* removing empty spaces */
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /* no argument followed by spaces */
    if ('\0' == *inPtr) {
        return -EINVAL;
    }

    /* Getting the next argument ie the dwell time */
    v = sscanf(inPtr, "%31s ", tempBuf);
    if (1 != v) return -EINVAL;

    v = kstrtos32(tempBuf, 10, &tempInt);
    if ( v < 0 || tempInt < 0) return -EINVAL;

    *pDwellTime = tempInt;

    /* point to the next argument */
    inPtr = strnchr(inPtr, strlen(inPtr), SPACE_ASCII_VALUE);
    /* no argument after the command */
    if (NULL == inPtr) return -EINVAL;
    /* removing empty spaces */
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /* no argument followed by spaces */
    if ('\0' == *inPtr) {
        return -EINVAL;
    }

    /* find the length of data */
    dataEnd = inPtr;
    while('\0' !=  *dataEnd) {
        dataEnd++;
    }
    *pBufLen = dataEnd - inPtr ;
    if (*pBufLen <= 0)  return -EINVAL;

    /* Allocate the number of bytes based on the number of input characters
       whether it is even or odd.
       if the number of input characters are even, then we need N/2 byte.
       if the number of input characters are odd, then we need do (N+1)/2 to
       compensate rounding off.
       For example, if N = 18, then (18 + 1)/2 = 9 bytes are enough.
       If N = 19, then we need 10 bytes, hence (19 + 1)/2 = 10 bytes */
    *pBuf = vos_mem_malloc((*pBufLen + 1)/2);
    if (NULL == *pBuf) {
        hddLog(VOS_TRACE_LEVEL_FATAL,
           "%s: vos_mem_alloc failed ", __func__);
        return -EINVAL;
    }

    /* the buffer received from the upper layer is character buffer,
       we need to prepare the buffer taking 2 characters in to a U8 hex decimal number
       for example 7f0000f0...form a buffer to contain 7f in 0th location, 00 in 1st
       and f0 in 3rd location */
    for (i = 0, j = 0; j < *pBufLen; j += 2) {
        if( j+1 == *pBufLen) {
             tempByte = hdd_parse_hex(inPtr[j]);
        } else {
              tempByte = (hdd_parse_hex(inPtr[j]) << 4) | (hdd_parse_hex(inPtr[j + 1]));
        }
        (*pBuf)[i++] = tempByte;
    }
    *pBufLen = i;
    return 0;
}

/*
  \brief hdd_sendactionframe() - send a user space supplied action frame

  \param - pAdapter - Adapter upon which the command was received
  \param - bssid - BSSID target of the action frame
  \param - channel - channel upon which to send the frame
  \param - dwell_time - amount of time to dwell when the frame is sent
  \param - payload_len - length of the payload
  \param - payload - payload of the frame

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
static int
hdd_sendactionframe(hdd_adapter_t *pAdapter, const tANI_U8 *bssid,
                    const tANI_U8 channel, const tANI_U8 dwell_time,
                    const tANI_U8 payload_len, const tANI_U8 *payload)
{
   struct ieee80211_channel chan;
   tANI_U8 frame_len;
   tANI_U8 *frame;
   struct ieee80211_hdr_3addr *hdr;
   u64 cookie;
   hdd_station_ctx_t *pHddStaCtx;
   hdd_context_t *pHddCtx;
   int ret = 0;
   tpSirMacVendorSpecificFrameHdr pVendorSpecific =
                   (tpSirMacVendorSpecificFrameHdr) payload;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)) || defined(WITH_BACKPORTS)
   struct cfg80211_mgmt_tx_params params;
#endif
   pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

   /* if not associated, no need to send action frame */
   if (eConnectionState_Associated != pHddStaCtx->conn_info.connState) {
      hddLog(VOS_TRACE_LEVEL_INFO, "%s: Not associated", __func__);
      ret = -EINVAL;
      goto exit;
   }

   /* if the target bssid is different from currently associated AP,
      then no need to send action frame */
   if (memcmp(bssid, pHddStaCtx->conn_info.bssId, VOS_MAC_ADDR_SIZE)) {
      hddLog(VOS_TRACE_LEVEL_INFO, "%s: STA is not associated to this AP",
             __func__);
      ret = -EINVAL;
      goto exit;
   }

   chan.center_freq = sme_ChnToFreq(channel);
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
   /* Check if it is specific action frame */
   if (pVendorSpecific->category == SIR_MAC_ACTION_VENDOR_SPECIFIC_CATEGORY) {
       static const tANI_U8 Oui[] = { 0x00, 0x00, 0xf0 };
       if (vos_mem_compare(pVendorSpecific->Oui, (void *) Oui, 3)) {
           /* if the channel number is different from operating channel then
              no need to send action frame */
           if (channel != 0) {
               if (channel != pHddStaCtx->conn_info.operationChannel) {
                   hddLog(VOS_TRACE_LEVEL_INFO,
                     "%s: channel(%d) is different from operating channel(%d)",
                     __func__, channel,
                     pHddStaCtx->conn_info.operationChannel);
                   ret = -EINVAL;
                   goto exit;
               }
               /* If channel number is specified and same as home channel,
                * ensure that action frame is sent immediately by cancelling
                * roaming scans. Otherwise large dwell times may cause long
                * delays in sending action frames.
                */
               sme_abortRoamScan(pHddCtx->hHal, pAdapter->sessionId);
           } else {
               /* 0 is accepted as current home channel, delayed
                * transmission of action frame is ok.
                */
               chan.center_freq =
                       sme_ChnToFreq(pHddStaCtx->conn_info.operationChannel);
           }
       }
   }
#endif //#if WLAN_FEATURE_ROAM_SCAN_OFFLOAD
   if (chan.center_freq == 0) {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s:invalid channel number %d",
              __func__, channel);
      ret = -EINVAL;
      goto exit;
   }

   frame_len = payload_len + 24;
   frame = vos_mem_malloc(frame_len);
   if (!frame) {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s:memory allocation failed", __func__);
      ret = -ENOMEM;
      goto exit;
   }
   vos_mem_zero(frame, frame_len);

   hdr = (struct ieee80211_hdr_3addr *)frame;
   hdr->frame_control =
      cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_ACTION);
   vos_mem_copy(hdr->addr1, bssid, VOS_MAC_ADDR_SIZE);
   vos_mem_copy(hdr->addr2, pAdapter->macAddressCurrent.bytes,
                VOS_MAC_ADDR_SIZE);
   vos_mem_copy(hdr->addr3, bssid, VOS_MAC_ADDR_SIZE);
   vos_mem_copy(hdr + 1, payload, payload_len);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)) || defined(WITH_BACKPORTS)
   params.chan = &chan;
   params.offchan = 0;
   params.wait = dwell_time;
   params.buf = frame;
   params.len = frame_len;
   params.no_cck = 1;
   params.dont_wait_for_ack = 1;
   ret = wlan_hdd_mgmt_tx(NULL, &pAdapter->wdev, &params, &cookie);
#else
   ret = wlan_hdd_mgmt_tx(NULL,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)) || defined(WITH_BACKPORTS)
                         &(pAdapter->wdev),
#else
                         pAdapter->dev,
#endif
                         &chan, 0,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)) && !defined(WITH_BACKPORTS)
                         NL80211_CHAN_HT20, 1,
#endif
                         dwell_time, frame, frame_len, 1, 1, &cookie );
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)*/

   vos_mem_free(frame);
 exit:
   return ret;
}

/*
  \brief hdd_parse_sendactionframe_v1() - parse version 1 of the
         SENDACTIONFRAME command

  This function parses the v1 SENDACTIONFRAME command with the format

      SENDACTIONFRAME xx:xx:xx:xx:xx:xx CH DW xxxxxx

  Where "xx:xx:xx:xx:xx:xx" is the Hex-ASCII representation of the
  BSSID, CH is the ASCII representation of the channel, DW is the
  ASCII representation of the dwell time, and xxxxxx is the Hex-ASCII
  payload.  For example

      SENDACTIONFRAME 00:0a:0b:11:22:33 48 40 aabbccddee

  \param - pAdapter - Adapter upon which the command was received
  \param - command - ASCII text command that was received

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
static int
hdd_parse_sendactionframe_v1(hdd_adapter_t *pAdapter, const char *command)
{
   tANI_U8 channel = 0;
   tANI_U8 dwell_time = 0;
   tANI_U8 payload_len = 0;
   tANI_U8 *payload = NULL;
   tSirMacAddr bssid;
   int ret;

   ret = hdd_parse_send_action_frame_v1_data(command, bssid, &channel,
                                             &dwell_time, &payload,
                                             &payload_len);
   if (ret) {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Failed to parse send action frame data", __func__);
   } else {
      ret = hdd_sendactionframe(pAdapter, bssid, channel, dwell_time,
                                payload_len, payload);
      vos_mem_free(payload);
   }

   return ret;
}

/*
  \brief hdd_parse_sendactionframe_v2() - parse version 2 of the
         SENDACTIONFRAME command

  This function parses the v2 SENDACTIONFRAME command with the format

      SENDACTIONFRAME <android_wifi_af_params>

  \param - pAdapter - Adapter upon which the command was received
  \param - command - command that was received, ASCII command followed
                     by binary data

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
static int
hdd_parse_sendactionframe_v2(hdd_adapter_t *pAdapter,
                             const char *command)
{
   struct android_wifi_af_params *params;
   tSirMacAddr bssid;
   int ret;

   /* params are large so keep off the stack */
   params = kmalloc(sizeof(*params), GFP_KERNEL);
   if (!params) return -ENOMEM;

   /* The params are located after "SENDACTIONFRAME " */
   memcpy(params, command + 16, sizeof(*params));

   if (!mac_pton(params->bssid, (u8 *)&bssid)) {
      hddLog(LOGE, "%s: MAC address parsing failed", __func__);
      ret = -EINVAL;
   } else {
      ret = hdd_sendactionframe(pAdapter, bssid, params->channel,
                                params->dwell_time, params->len, params->data);
   }
   kfree(params);
   return ret;
}

/*
  \brief hdd_parse_sendactionframe() - parse the SENDACTIONFRAME command

  There are two different versions of the SENDACTIONFRAME command.
  Version 1 of the command contains a parameter list that is ASCII
  characters whereas version 2 contains a combination of ASCII and
  binary payload.  Determine if a version 1 or a version 2 command is
  being parsed by examining the parameters, and then dispatch the
  parser that is appropriate for the version of the command.

  \param - pAdapter - Adapter upon which the command was received
  \param - command - command that was received

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
static int
hdd_parse_sendactionframe(hdd_adapter_t *pAdapter, const char *command)
{
   int ret;

   /* both versions start with "SENDACTIONFRAME "
    * v1 has a bssid and other parameters as an ASCII string
    *    SENDACTIONFRAME xx:xx:xx:xx:xx:xx CH DWELL LEN FRAME
    * v2 has a C struct
    *    SENDACTIONFRAME <binary c struct>
    *
    * The first field in the v2 struct is also the bssid in ASCII.
    * But in the case of a v2 message the BSSID is NUL-terminated.
    * Hence we can peek at that offset to see if this is V1 or V2
    * SENDACTIONFRAME xx:xx:xx:xx:xx:xx*
    *           111111111122222222223333
    * 0123456789012345678901234567890123
    */
   if (command[33]) {
      ret = hdd_parse_sendactionframe_v1(pAdapter, command);
   } else {
      ret = hdd_parse_sendactionframe_v2(pAdapter, command);
   }

   return ret;
}

static void hdd_getBand_helper(hdd_context_t *pHddCtx, int *pBand)
{
    eCsrBand band = -1;
    sme_GetFreqBand((tHalHandle)(pHddCtx->hHal), &band);
    switch (band) {
    case eCSR_BAND_ALL:
        *pBand = WLAN_HDD_UI_BAND_AUTO;
        break;

    case eCSR_BAND_24:
        *pBand = WLAN_HDD_UI_BAND_2_4_GHZ;
        break;

    case eCSR_BAND_5G:
        *pBand = WLAN_HDD_UI_BAND_5_GHZ;
        break;

    default:
        hddLog(LOGW, FL("Invalid Band %d"), band);
        *pBand = -1;
        break;
    }
}


/**
 * hdd_parse_channellist() - HDD Parse channel list
 * @pValue:     Pointer to input data
 * @pChannelList: Pointer to input channel list
 * @pNumChannels: Pointer to number of roam scan channels
 *
 * This function parses the channel list passed in the format
 * SETROAMSCANCHANNELS<space><Number of channels><space>Channel 1<space>
 * Channel 2<space>Channel N
 * if the Number of channels (N) does not match with the actual number of
 * channels passed then take the minimum of N and count of (Ch1, Ch2, ...Ch M)
 * For example, if SETROAMSCANCHANNELS 3 36 40 44 48, only 36, 40 and 44 shall
 * be taken.
 * If SETROAMSCANCHANNELS 5 36 40 44 48, ignore 5 and take 36, 40, 44 and 48.
 * This function does not take care of removing duplicate channels from the list
 *
 * Return: 0 for success non-zero for failure
 */
static int
hdd_parse_channellist(const tANI_U8 *pValue, tANI_U8 *pChannelList,
                      tANI_U8 *pNumChannels)
{
    const tANI_U8 *inPtr = pValue;
    int tempInt;
    int j = 0;
    int v = 0;
    char buf[32];

    inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
    /* no argument after the command */
    if (NULL == inPtr) {
        return -EINVAL;
    } else if (SPACE_ASCII_VALUE != *inPtr) {
        /* no space after the command */
        return -EINVAL;
    }

    /* removing empty spaces */
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr)) inPtr++;

    /* no argument followed by spaces */
    if ('\0' == *inPtr) {
        return -EINVAL;
    }

    /*getting the first argument ie the number of channels*/
    v = sscanf(inPtr, "%31s ", buf);
    if (1 != v) return -EINVAL;

    v = kstrtos32(buf, 10, &tempInt);
    if ((v < 0) ||
        (tempInt <= 0) ||
        (tempInt > WNI_CFG_VALID_CHANNEL_LIST_LEN)) {
       return -EINVAL;
    }

    *pNumChannels = tempInt;

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
               "Number of channels are: %d", *pNumChannels);

    for (j = 0; j < (*pNumChannels); j++) {
        /* inPtr pointing to the beginning of first space after number of
           channels*/
        inPtr = strpbrk( inPtr, " " );
        /* no channel list after the number of channels argument */
        if (NULL == inPtr) {
            if (0 != j) {
                *pNumChannels = j;
                return 0;
            } else {
                return -EINVAL;
            }
        }

        /* removing empty space */
        while ((SPACE_ASCII_VALUE == *inPtr) && ('\0' != *inPtr)) inPtr++;

        /*no channel list after the number of channels argument and spaces*/
        if ('\0' == *inPtr) {
            if (0 != j) {
                *pNumChannels = j;
                return 0;
            } else {
                return -EINVAL;
            }
        }

        v = sscanf(inPtr, "%31s ", buf);
        if (1 != v) return -EINVAL;

        v = kstrtos32(buf, 10, &tempInt);
        if ((v < 0) ||
            (tempInt <= 0) ||
            (tempInt > WNI_CFG_CURRENT_CHANNEL_STAMAX)) {
           return -EINVAL;
        }
        pChannelList[j] = tempInt;

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Channel %d added to preferred channel list",
                   pChannelList[j] );
    }

    return 0;
}

/*
  \brief hdd_parse_set_roam_scan_channels_v1() - parse version 1 of the
  SETROAMSCANCHANNELS command

  This function parses the v1 SETROAMSCANCHANNELS command with the format

      SETROAMSCANCHANNELS N C1 C2 ... Cn

  Where "N" is the ASCII representation of the number of channels and
  C1 thru Cn is the ASCII representation of the channels.  For example

      SETROAMSCANCHANNELS 4 36 40 44 48

  \param - pAdapter - Adapter upon which the command was received
  \param - command - ASCII text command that was received

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
static int
hdd_parse_set_roam_scan_channels_v1(hdd_adapter_t *pAdapter,
                                    const char *command)
{
   tANI_U8 channel_list[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
   tANI_U8 num_chan = 0;
   eHalStatus status;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   int ret;

   ret = hdd_parse_channellist(command, channel_list, &num_chan);
   if (ret) {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: Failed to parse channel list information", __func__);
      goto exit;
   }

   MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                    TRACE_CODE_HDD_SETROAMSCANCHANNELS_IOCTL,
                    pAdapter->sessionId, num_chan));

   if (num_chan > WNI_CFG_VALID_CHANNEL_LIST_LEN) {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: number of channels (%d) supported exceeded max (%d)",
                  __func__, num_chan, WNI_CFG_VALID_CHANNEL_LIST_LEN);
      ret = -EINVAL;
      goto exit;
   }

   status = sme_ChangeRoamScanChannelList(pHddCtx->hHal, pAdapter->sessionId,
                                          channel_list, num_chan);
   if (eHAL_STATUS_SUCCESS != status) {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: Failed to update channel list information", __func__);
      ret = -EINVAL;
      goto exit;
   }
 exit:
   return ret;
}

/*
  \brief hdd_parse_set_roam_scan_channels_v2() - parse version 2 of the
  SETROAMSCANCHANNELS command

  This function parses the v2 SETROAMSCANCHANNELS command with the format

      SETROAMSCANCHANNELS [N][C1][C2][Cn]

  The command begins with SETROAMSCANCHANNELS followed by a space, but
  what follows the space is an array of u08 parameters.  For example

      SETROAMSCANCHANNELS [0x04 0x24 0x28 0x2c 0x30]

  \param - pAdapter - Adapter upon which the command was received
  \param - command - command that was received, ASCII command followed
                     by binary data

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
static int
hdd_parse_set_roam_scan_channels_v2(hdd_adapter_t *pAdapter,
                                    const char *command)
{
   const tANI_U8 *value;
   tANI_U8 channel_list[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
   tANI_U8 channel;
   tANI_U8 num_chan;
   int i;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   eHalStatus status;
   int ret = 0;

   /* array of values begins after "SETROAMSCANCHANNELS " */
   value = command + 20;

   num_chan = *value++;
   if (num_chan > WNI_CFG_VALID_CHANNEL_LIST_LEN) {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: number of channels (%d) supported exceeded max (%d)",
                __func__, num_chan, WNI_CFG_VALID_CHANNEL_LIST_LEN);
      ret = -EINVAL;
      goto exit;
   }

   MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                    TRACE_CODE_HDD_SETROAMSCANCHANNELS_IOCTL,
                    pAdapter->sessionId, num_chan));

   for (i = 0; i < num_chan; i++) {
      channel = *value++;
      if (channel > WNI_CFG_CURRENT_CHANNEL_STAMAX) {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: index %d invalid channel %d", __func__, i, channel);
         ret = -EINVAL;
         goto exit;
      }
      channel_list[i] = channel;
   }
   status = sme_ChangeRoamScanChannelList(pHddCtx->hHal, pAdapter->sessionId,
                                          channel_list, num_chan);
   if (eHAL_STATUS_SUCCESS != status) {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: Failed to update channel list information", __func__);
      ret = -EINVAL;
      goto exit;
   }
 exit:
   return ret;
}

/*
  \brief hdd_parse_set_roam_scan_channels() - parse the
  SETROAMSCANCHANNELS command

  There are two different versions of the SETROAMSCANCHANNELS command.
  Version 1 of the command contains a parameter list that is ASCII
  characters whereas version 2 contains a binary payload.  Determine
  if a version 1 or a version 2 command is being parsed by examining
  the parameters, and then dispatch the parser that is appropriate for
  the command.

  \param - pAdapter - Adapter upon which the command was received
  \param - command - command that was received

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
static int
hdd_parse_set_roam_scan_channels(hdd_adapter_t *pAdapter,
                                 const char *command)
{
   const char *cursor;
   char ch;
   bool v1;
   int ret;

   /* start after "SETROAMSCANCHANNELS " */
   cursor = command + 20;

   /* assume we have a version 1 command until proven otherwise */
   v1 = true;

   /* v1 params will only contain ASCII digits and space */
   while ((ch = *cursor++) && v1) {
      if (!(isdigit(ch) || isspace(ch))) {
         v1 = false;
      }
   }
   if (v1) {
      ret = hdd_parse_set_roam_scan_channels_v1(pAdapter, command);
   } else {
      ret = hdd_parse_set_roam_scan_channels_v2(pAdapter, command);
   }

   return ret;
}

#endif /* WLAN_FEATURE_VOWIFI_11R || FEATURE_WLAN_ESE || FEATURE_WLAN_LFR */

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
/**---------------------------------------------------------------------------

  \brief hdd_parse_plm_cmd() - HDD Parse Plm command

  This function parses the plm command passed in the format
  CCXPLMREQ<space><enable><space><dialog_token><space>
  <meas_token><space><num_of_bursts><space><burst_int><space>
  <measu duration><space><burst_len><space><desired_tx_pwr>
  <space><multcast_addr><space><number_of_channels>
  <space><channel_numbers>

  \param  - pValue Pointer to input data
  \param  - pPlmRequest Pointer to output struct tpSirPlmReq

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
static eHalStatus hdd_parse_plm_cmd(tANI_U8 *pValue, tSirPlmReq *pPlmRequest)
{
     tANI_U8 *cmdPtr = NULL;
     int count, content = 0, ret = 0;
     char buf[32];

     /* moving to argument list */
     cmdPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
     if (NULL == cmdPtr)
        return eHAL_STATUS_FAILURE;

     /*no space after the command*/
     if (SPACE_ASCII_VALUE != *cmdPtr)
        return eHAL_STATUS_FAILURE;

     /*removing empty spaces*/
     while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
        cmdPtr++;

     /* START/STOP PLM req */
     ret = sscanf(cmdPtr, "%31s ", buf);
     if (1 != ret) return eHAL_STATUS_FAILURE;

     ret = kstrtos32(buf, 10, &content);
     if ( ret < 0) return eHAL_STATUS_FAILURE;

     pPlmRequest->enable = content;
     cmdPtr = strpbrk(cmdPtr, " ");

     if (NULL == cmdPtr)
        return eHAL_STATUS_FAILURE;

     /*removing empty spaces*/
     while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
        cmdPtr++;

     /* Dialog token of radio meas req containing meas reqIE */
     ret = sscanf(cmdPtr, "%31s ", buf);
     if (1 != ret) return eHAL_STATUS_FAILURE;

     ret = kstrtos32(buf, 10, &content);
     if ( ret < 0) return eHAL_STATUS_FAILURE;

     pPlmRequest->diag_token = content;
     hddLog(VOS_TRACE_LEVEL_DEBUG, "diag token %d", pPlmRequest->diag_token);
     cmdPtr = strpbrk(cmdPtr, " ");

     if (NULL == cmdPtr)
        return eHAL_STATUS_FAILURE;

     /*removing empty spaces*/
     while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
         cmdPtr++;

     /* measurement token of meas req IE */
     ret = sscanf(cmdPtr, "%31s ", buf);
     if (1 != ret) return eHAL_STATUS_FAILURE;

     ret = kstrtos32(buf, 10, &content);
     if ( ret < 0) return eHAL_STATUS_FAILURE;

     pPlmRequest->meas_token = content;
     hddLog(VOS_TRACE_LEVEL_DEBUG, "meas token %d", pPlmRequest->meas_token);

     hddLog(VOS_TRACE_LEVEL_ERROR,
            "PLM req %s", pPlmRequest->enable ? "START" : "STOP");
     if (pPlmRequest->enable) {

        cmdPtr = strpbrk(cmdPtr, " ");

        if (NULL == cmdPtr)
           return eHAL_STATUS_FAILURE;

        /*removing empty spaces*/
        while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
             cmdPtr++;

        /* total number of bursts after which STA stops sending */
        ret = sscanf(cmdPtr, "%31s ", buf);
        if (1 != ret) return eHAL_STATUS_FAILURE;

        ret = kstrtos32(buf, 10, &content);
        if ( ret < 0) return eHAL_STATUS_FAILURE;

        if (content < 0)
           return eHAL_STATUS_FAILURE;

        pPlmRequest->numBursts= content;
        hddLog(VOS_TRACE_LEVEL_DEBUG, "num burst %d", pPlmRequest->numBursts);
        cmdPtr = strpbrk(cmdPtr, " ");

        if (NULL == cmdPtr)
           return eHAL_STATUS_FAILURE;

        /* removing empty spaces */
        while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
            cmdPtr++;

        /* burst interval in seconds */
        ret = sscanf(cmdPtr, "%31s ", buf);
        if (1 != ret) return eHAL_STATUS_FAILURE;

        ret = kstrtos32(buf, 10, &content);
        if ( ret < 0) return eHAL_STATUS_FAILURE;

        if (content <= 0)
           return eHAL_STATUS_FAILURE;

        pPlmRequest->burstInt = content;
        hddLog(VOS_TRACE_LEVEL_DEBUG, "burst Int %d", pPlmRequest->burstInt);
        cmdPtr = strpbrk(cmdPtr, " ");

        if (NULL == cmdPtr)
           return eHAL_STATUS_FAILURE;

        /*removing empty spaces*/
        while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
             cmdPtr++;

        /* Meas dur in TU's,STA goes off-ch and transmit PLM bursts */
        ret = sscanf(cmdPtr, "%31s ", buf);
        if (1 != ret) return eHAL_STATUS_FAILURE;

        ret = kstrtos32(buf, 10, &content);
        if ( ret < 0) return eHAL_STATUS_FAILURE;

        if (content <= 0)
           return eHAL_STATUS_FAILURE;

        pPlmRequest->measDuration = content;
        hddLog(VOS_TRACE_LEVEL_DEBUG, "measDur %d", pPlmRequest->measDuration);
        cmdPtr = strpbrk(cmdPtr, " ");

        if (NULL == cmdPtr)
           return eHAL_STATUS_FAILURE;

        /*removing empty spaces*/
        while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
            cmdPtr++;

        /* burst length of PLM bursts */
        ret = sscanf(cmdPtr, "%31s ", buf);
        if (1 != ret) return eHAL_STATUS_FAILURE;

        ret = kstrtos32(buf, 10, &content);
        if ( ret < 0) return eHAL_STATUS_FAILURE;

        if (content <= 0)
           return eHAL_STATUS_FAILURE;

        pPlmRequest->burstLen = content;
        hddLog(VOS_TRACE_LEVEL_DEBUG, "burstLen %d", pPlmRequest->burstLen);
        cmdPtr = strpbrk(cmdPtr, " ");

        if (NULL == cmdPtr)
           return eHAL_STATUS_FAILURE;

        /*removing empty spaces*/
        while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
             cmdPtr++;

        /* desired tx power for transmission of PLM bursts */
        ret = sscanf(cmdPtr, "%31s ", buf);
        if (1 != ret) return eHAL_STATUS_FAILURE;

        ret = kstrtos32(buf, 10, &content);
        if ( ret < 0) return eHAL_STATUS_FAILURE;

        if (content <= 0)
           return eHAL_STATUS_FAILURE;

        pPlmRequest->desiredTxPwr = content;
        hddLog(VOS_TRACE_LEVEL_DEBUG,
               "desiredTxPwr %d", pPlmRequest->desiredTxPwr);

        for (count = 0; count < VOS_MAC_ADDR_SIZE; count++)
        {
            cmdPtr = strpbrk(cmdPtr, " ");

            if (NULL == cmdPtr)
               return eHAL_STATUS_FAILURE;

            /*removing empty spaces*/
            while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
                cmdPtr++;

            ret = sscanf(cmdPtr, "%31s ", buf);
            if (1 != ret) return eHAL_STATUS_FAILURE;

            ret = kstrtos32(buf, 16, &content);
            if ( ret < 0) return eHAL_STATUS_FAILURE;

            pPlmRequest->macAddr[count] = content;
        }

        hddLog(VOS_TRACE_LEVEL_DEBUG, "MC addr "MAC_ADDRESS_STR,
                          MAC_ADDR_ARRAY(pPlmRequest->macAddr));

        cmdPtr = strpbrk(cmdPtr, " ");

        if (NULL == cmdPtr)
           return eHAL_STATUS_FAILURE;

        /*removing empty spaces*/
        while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
            cmdPtr++;

        /* number of channels */
        ret = sscanf(cmdPtr, "%31s ", buf);
        if (1 != ret) return eHAL_STATUS_FAILURE;

        ret = kstrtos32(buf, 10, &content);
        if ( ret < 0) return eHAL_STATUS_FAILURE;

        if (content < 0)
           return eHAL_STATUS_FAILURE;

        pPlmRequest->plmNumCh = content;
        hddLog(VOS_TRACE_LEVEL_DEBUG, "numch %d", pPlmRequest->plmNumCh);

        /* Channel numbers */
        for (count = 0; count < pPlmRequest->plmNumCh; count++)
        {
             cmdPtr = strpbrk(cmdPtr, " ");

             if (NULL == cmdPtr)
                return eHAL_STATUS_FAILURE;

             /*removing empty spaces*/
             while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
                cmdPtr++;

             ret = sscanf(cmdPtr, "%31s ", buf);
             if (1 != ret) return eHAL_STATUS_FAILURE;

             ret = kstrtos32(buf, 10, &content);
             if ( ret < 0) return eHAL_STATUS_FAILURE;

             if (content <= 0)
                return eHAL_STATUS_FAILURE;

             pPlmRequest->plmChList[count]= content;
             hddLog(VOS_TRACE_LEVEL_DEBUG, " ch- %d",
                    pPlmRequest->plmChList[count]);
         }
     } /* If PLM START */

     return eHAL_STATUS_SUCCESS;
}
#endif
#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
static void wlan_hdd_ready_to_extwow(void *callbackContext,
                                             boolean is_success)
{
    hdd_context_t *pHddCtx = (hdd_context_t *)callbackContext;
    int rc;

    rc = wlan_hdd_validate_context(pHddCtx);
    if (0 != rc) {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
       return;
    }
    pHddCtx->ext_wow_should_suspend = is_success;
    complete(&pHddCtx->ready_to_extwow);
}

static int hdd_enable_ext_wow(hdd_adapter_t *pAdapter,
                               tpSirExtWoWParams arg_params)
{
    tSirExtWoWParams params;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    hdd_context_t  *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    int rc;

    vos_mem_copy(&params, arg_params, sizeof(params));

    INIT_COMPLETION(pHddCtx->ready_to_extwow);

    halStatus = sme_ConfigureExtWoW(hHal, &params,
                &wlan_hdd_ready_to_extwow, pHddCtx);
    if (eHAL_STATUS_SUCCESS != halStatus) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("sme_ConfigureExtWoW returned failure %d"), halStatus);
        return -EPERM;
    }

    rc = wait_for_completion_timeout(&pHddCtx->ready_to_extwow,
                             msecs_to_jiffies(WLAN_WAIT_TIME_READY_TO_EXTWOW));
    if (!rc) {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Failed to get ready to extwow", __func__);
        return -EPERM;
    }

    if (pHddCtx->ext_wow_should_suspend) {
       if (pHddCtx->cfg_ini->extWowGotoSuspend) {
          VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "%s: Received ready to ExtWoW. Going to suspend", __func__);

          wlan_hdd_cfg80211_suspend_wlan(pHddCtx->wiphy, NULL);
          wlan_hif_pci_suspend();
       }
    } else {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              "%s: Received ready to ExtWoW failure", __func__);
        return -EPERM;
    }

    return 0;
}

static int hdd_enable_ext_wow_parser(hdd_adapter_t *pAdapter, int vdev_id,
                                                                  int value)
{
   tSirExtWoWParams params;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   int rc;

   rc = wlan_hdd_validate_context(pHddCtx);
   if (0 != rc) {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
       return -EINVAL;
   }

   if (value < EXT_WOW_TYPE_APP_TYPE1 || value > EXT_WOW_TYPE_APP_TYPE1_2 ) {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid type"));
       return -EINVAL;
   }

   if (value == EXT_WOW_TYPE_APP_TYPE1 &&
        pHddCtx->is_extwow_app_type1_param_set)
        params.type = value;
   else if (value == EXT_WOW_TYPE_APP_TYPE2 &&
        pHddCtx->is_extwow_app_type2_param_set)
        params.type = value;
   else if (value == EXT_WOW_TYPE_APP_TYPE1_2 &&
        pHddCtx->is_extwow_app_type1_param_set &&
        pHddCtx->is_extwow_app_type2_param_set)
        params.type = value;
   else {
        hddLog(VOS_TRACE_LEVEL_ERROR,
           FL("Set app params before enable it value %d"),value);
        return -EINVAL;
   }

   params.vdev_id = vdev_id;
   params.wakeup_pin_num = pHddCtx->cfg_ini->extWowApp1WakeupPinNumber |
                      (pHddCtx->cfg_ini->extWowApp2WakeupPinNumber << 8);

   return hdd_enable_ext_wow(pAdapter, &params);
}

static int hdd_set_app_type1_params(tHalHandle hHal,
                         tpSirAppType1Params arg_params)
{
    tSirAppType1Params params;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;

    vos_mem_copy(&params, arg_params, sizeof(params));

    halStatus = sme_ConfigureAppType1Params(hHal, &params);
    if (eHAL_STATUS_SUCCESS != halStatus) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
             FL("sme_ConfigureAppType1Params returned failure %d"), halStatus);
        return -EPERM;
    }

    return 0;
}

static int hdd_set_app_type1_parser(hdd_adapter_t *pAdapter,
                                             char *arg, int len)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    char id[20], password[20];
    tSirAppType1Params params;
    int rc, i;

    rc = wlan_hdd_validate_context(pHddCtx);
    if (0 != rc) {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
       return -EINVAL;
    }

    if (2 != sscanf(arg, "%8s %16s", id, password)) {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("Invalid Number of arguments"));
       return -EINVAL;
    }

    memset(&params, 0, sizeof(tSirAppType1Params));
    params.vdev_id = pAdapter->sessionId;
    for (i = 0; i < ETHER_ADDR_LEN; i++)
        params.wakee_mac_addr[i] = pAdapter->macAddressCurrent.bytes[i];

    params.id_length = strlen(id);
    vos_mem_copy(params.identification_id, id, params.id_length);
    params.pass_length = strlen(password);
    vos_mem_copy(params.password, password, params.pass_length);

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
        "%s: %d %pM %.8s %u %.16s %u",
        __func__, params.vdev_id, params.wakee_mac_addr,
        params.identification_id, params.id_length,
        params.password, params.pass_length );

    return hdd_set_app_type1_params(hHal, &params);
}

static int hdd_set_app_type2_params(tHalHandle hHal,
                          tpSirAppType2Params arg_params)
{
    tSirAppType2Params params;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;

    vos_mem_copy(&params, arg_params, sizeof(params));

    halStatus = sme_ConfigureAppType2Params(hHal, &params);
    if (eHAL_STATUS_SUCCESS != halStatus)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
             FL("sme_ConfigureAppType2Params returned failure %d"), halStatus);
        return -EPERM;
    }

    return 0;
}

static int hdd_set_app_type2_parser(hdd_adapter_t *pAdapter,
                            char *arg, int len)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    char mac_addr[20], rc4_key[20];
    unsigned int gateway_mac[6], i;
    tSirAppType2Params params;
    int ret;

    ret = wlan_hdd_validate_context(pHddCtx);
    if (0 != ret) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
        return -EINVAL;
    }

    memset(&params, 0, sizeof(tSirAppType2Params));

    ret = sscanf(arg, "%17s %16s %x %x %x %u %u %hu %hu %u %u %u %u %u %u",
        mac_addr, rc4_key, (unsigned int *)&params.ip_id,
        (unsigned int*)&params.ip_device_ip,
        (unsigned int*)&params.ip_server_ip,
        (unsigned int*)&params.tcp_seq, (unsigned int*)&params.tcp_ack_seq,
        (uint16_t*)&params.tcp_src_port,
        (uint16_t*)&params.tcp_dst_port,
        (unsigned int*)&params.keepalive_init,
        (unsigned int*)&params.keepalive_min,
        (unsigned int*)&params.keepalive_max,
        (unsigned int*)&params.keepalive_inc,
        (unsigned int*)&params.tcp_tx_timeout_val,
        (unsigned int*)&params.tcp_rx_timeout_val);


    if (ret != 15 && ret != 7) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "Invalid Number of arguments");
        return -EINVAL;
    }

    if (6 != sscanf(mac_addr, "%02x:%02x:%02x:%02x:%02x:%02x", &gateway_mac[0],
             &gateway_mac[1], &gateway_mac[2], &gateway_mac[3],
             &gateway_mac[4], &gateway_mac[5])) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "Invalid MacAddress Input %s", mac_addr);
        return -EINVAL;
    }

    if (params.tcp_src_port > WLAN_HDD_MAX_TCP_PORT ||
           params.tcp_dst_port > WLAN_HDD_MAX_TCP_PORT) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "Invalid TCP Port Number");
        return -EINVAL;
    }

    for (i = 0; i < ETHER_ADDR_LEN; i++)
        params.gateway_mac[i] = (uint8_t) gateway_mac[i];

    params.rc4_key_len = strlen(rc4_key);
    vos_mem_copy(params.rc4_key, rc4_key, params.rc4_key_len);

    params.vdev_id = pAdapter->sessionId;
    params.tcp_src_port = (params.tcp_src_port != 0)?
        params.tcp_src_port : pHddCtx->cfg_ini->extWowApp2TcpSrcPort;
    params.tcp_dst_port = (params.tcp_dst_port != 0)?
        params.tcp_dst_port : pHddCtx->cfg_ini->extWowApp2TcpDstPort;
    params.keepalive_init = (params.keepalive_init != 0)?
        params.keepalive_init : pHddCtx->cfg_ini->extWowApp2KAInitPingInterval;
    params.keepalive_min = (params.keepalive_min != 0)?
        params.keepalive_min : pHddCtx->cfg_ini->extWowApp2KAMinPingInterval;
    params.keepalive_max = (params.keepalive_max != 0)?
        params.keepalive_max : pHddCtx->cfg_ini->extWowApp2KAMaxPingInterval;
    params.keepalive_inc = (params.keepalive_inc != 0)?
        params.keepalive_inc : pHddCtx->cfg_ini->extWowApp2KAIncPingInterval;
    params.tcp_tx_timeout_val = (params.tcp_tx_timeout_val != 0)?
        params.tcp_tx_timeout_val : pHddCtx->cfg_ini->extWowApp2TcpTxTimeout;
    params.tcp_rx_timeout_val = (params.tcp_rx_timeout_val != 0)?
        params.tcp_rx_timeout_val : pHddCtx->cfg_ini->extWowApp2TcpRxTimeout;

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
        "%s: %pM %.16s %u %u %u %u %u %u %u %u %u %u %u %u %u",
        __func__, gateway_mac, rc4_key, params.ip_id, params.ip_device_ip,
        params.ip_server_ip, params.tcp_seq, params.tcp_ack_seq,
        params.tcp_src_port, params.tcp_dst_port, params.keepalive_init,
        params.keepalive_min, params.keepalive_max,
        params.keepalive_inc, params.tcp_tx_timeout_val,
        params.tcp_rx_timeout_val);

    return hdd_set_app_type2_params(hHal, &params);
}

#endif

int wlan_hdd_set_mc_rate(hdd_adapter_t *pAdapter, int targetRate)
{
   tSirRateUpdateInd rateUpdate = {0};
   eHalStatus status;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   hdd_config_t *pConfig = NULL;

   if (pHddCtx == NULL) {
      hddLog(LOGE, FL("HDD context is null"));
      return -EINVAL;
   }

   if ((WLAN_HDD_IBSS != pAdapter->device_mode) &&
       (WLAN_HDD_SOFTAP != pAdapter->device_mode) &&
       (WLAN_HDD_INFRA_STATION != pAdapter->device_mode)) {
      hddLog(LOGE, FL("Received SETMCRATE cmd in invalid device mode %s(%d)"),
             hdd_device_mode_to_string(pAdapter->device_mode),
             pAdapter->device_mode);
      hddLog(LOGE,
             FL("SETMCRATE cmd is allowed only in STA, IBSS or SOFTAP mode"));
      return -EINVAL;
   }

   pConfig = pHddCtx->cfg_ini;
   rateUpdate.nss = (pConfig->enable2x2 == 0) ? 0 : 1;
   rateUpdate.dev_mode = pAdapter->device_mode;
   rateUpdate.mcastDataRate24GHz = targetRate;
   rateUpdate.mcastDataRate24GHzTxFlag = 1;
   rateUpdate.mcastDataRate5GHz = targetRate;
   rateUpdate.bcastDataRate = -1;
   memcpy(rateUpdate.bssid, pAdapter->macAddressCurrent.bytes,
      sizeof(rateUpdate.bssid));
   hddLog(LOG1, FL("MC Target rate %d, mac = %pM, dev_mode %s(%d)"),
      rateUpdate.mcastDataRate24GHz, rateUpdate.bssid,
      hdd_device_mode_to_string(pAdapter->device_mode),
      pAdapter->device_mode);

   status = sme_SendRateUpdateInd(pHddCtx->hHal, &rateUpdate);
   if (eHAL_STATUS_SUCCESS != status) {
      hddLog(LOGE, FL("SETMCRATE failed"));
      return -EFAULT;
   }
   return 0;
}

/**---------------------------------------------------------------------------

  \brief hdd_parse_setmaxtxpower_command() - HDD Parse MAXTXPOWER command

  This function parses the MAXTXPOWER command passed in the format
  MAXTXPOWER<space>X(Tx power in dbm)
  For example input commands:
  1) MAXTXPOWER -8 -> This is translated into set max TX power to -8 dbm
  2) MAXTXPOWER -23 -> This is translated into set max TX power to -23 dbm

  \param  - pValue Pointer to MAXTXPOWER command
  \param  - pDbm Pointer to tx power

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
static int hdd_parse_setmaxtxpower_command(tANI_U8 *pValue, int *pTxPower)
{
    tANI_U8 *inPtr = pValue;
    int tempInt;
    int v = 0;
    *pTxPower = 0;

    inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
    /*no argument after the command*/
    if (NULL == inPtr)
    {
        return -EINVAL;
    }

    /*no space after the command*/
    else if (SPACE_ASCII_VALUE != *inPtr)
    {
        return -EINVAL;
    }

    /*removing empty spaces*/
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr)) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr)
    {
        return 0;
    }

    v = kstrtos32(inPtr, 10, &tempInt);

    /* Range checking for passed parameter */
    if ( (tempInt < HDD_MIN_TX_POWER) ||
         (tempInt > HDD_MAX_TX_POWER) )
    {
       return -EINVAL;
    }

    *pTxPower = tempInt;

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
       "SETMAXTXPOWER: %d", *pTxPower);

    return 0;
} /*End of hdd_parse_setmaxtxpower_command*/


static int hdd_get_dwell_time(hdd_config_t *pCfg, tANI_U8 *command, char *extra, tANI_U8 n, tANI_U8 *len)
{
    int ret = 0;

    if (!pCfg || !command || !extra || !len)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s: argument passed for GETDWELLTIME is incorrect", __func__);
        ret = -EINVAL;
        return ret;
    }

    if (strncmp(command, "GETDWELLTIME ACTIVE MAX", 23) == 0)
    {
        *len = scnprintf(extra, n, "GETDWELLTIME ACTIVE MAX %u\n",
                (int)pCfg->nActiveMaxChnTime);
        return ret;
    }
    else if (strncmp(command, "GETDWELLTIME ACTIVE MIN", 23) == 0)
    {
        *len = scnprintf(extra, n, "GETDWELLTIME ACTIVE MIN %u\n",
                (int)pCfg->nActiveMinChnTime);
        return ret;
    }
    else if (strncmp(command, "GETDWELLTIME PASSIVE MAX", 24) == 0)
    {
        *len = scnprintf(extra, n, "GETDWELLTIME PASSIVE MAX %u\n",
                (int)pCfg->nPassiveMaxChnTime);
        return ret;
    }
    else if (strncmp(command, "GETDWELLTIME PASSIVE MIN", 24) == 0)
    {
        *len = scnprintf(extra, n, "GETDWELLTIME PASSIVE MIN %u\n",
                (int)pCfg->nPassiveMinChnTime);
        return ret;
    }
    else if (strncmp(command, "GETDWELLTIME", 12) == 0)
    {
        *len = scnprintf(extra, n, "GETDWELLTIME %u \n",
                (int)pCfg->nActiveMaxChnTime);
        return ret;
    }
    else
    {
        ret = -EINVAL;
    }

    return ret;
}

static int hdd_set_dwell_time(hdd_adapter_t *pAdapter, tANI_U8 *command)
{
    tHalHandle hHal;
    hdd_config_t *pCfg;
    tANI_U8 *value = command;
    tSmeConfigParams smeConfig;
    int val = 0, ret = 0, temp = 0;

    if (!pAdapter || !command || !(pCfg = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini)
        || !(hHal = (WLAN_HDD_GET_HAL_CTX(pAdapter))))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
         "%s: argument passed for SETDWELLTIME is incorrect", __func__);
        ret = -EINVAL;
        return ret;
    }

    vos_mem_zero(&smeConfig, sizeof(smeConfig));
    sme_GetConfigParam(hHal, &smeConfig);

    if (strncmp(command, "SETDWELLTIME ACTIVE MAX", 23) == 0 )
    {
        value = value + 24;
        temp = kstrtou32(value, 10, &val);
        if (temp != 0 || val < CFG_ACTIVE_MAX_CHANNEL_TIME_MIN ||
                         val > CFG_ACTIVE_MAX_CHANNEL_TIME_MAX )
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s: argument passed for SETDWELLTIME ACTIVE MAX is incorrect", __func__);
            ret = -EFAULT;
            return ret;
        }
        pCfg->nActiveMaxChnTime = val;
        smeConfig.csrConfig.nActiveMaxChnTime = val;
        sme_UpdateConfig(hHal, &smeConfig);
    }
    else if (strncmp(command, "SETDWELLTIME ACTIVE MIN", 23) == 0)
    {
        value = value + 24;
        temp = kstrtou32(value, 10, &val);
        if (temp !=0 || val < CFG_ACTIVE_MIN_CHANNEL_TIME_MIN  ||
                        val > CFG_ACTIVE_MIN_CHANNEL_TIME_MAX )
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s: argument passed for SETDWELLTIME ACTIVE MIN is incorrect", __func__);
            ret = -EFAULT;
            return ret;
        }
        pCfg->nActiveMinChnTime = val;
        smeConfig.csrConfig.nActiveMinChnTime = val;
        sme_UpdateConfig(hHal, &smeConfig);
    }
    else if (strncmp(command, "SETDWELLTIME PASSIVE MAX", 24) == 0)
    {
        value = value + 25;
        temp = kstrtou32(value, 10, &val);
        if (temp != 0 || val < CFG_PASSIVE_MAX_CHANNEL_TIME_MIN ||
                         val > CFG_PASSIVE_MAX_CHANNEL_TIME_MAX )
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s: argument passed for SETDWELLTIME PASSIVE MAX is incorrect", __func__);
            ret = -EFAULT;
            return ret;
        }
        pCfg->nPassiveMaxChnTime = val;
        smeConfig.csrConfig.nPassiveMaxChnTime = val;
        sme_UpdateConfig(hHal, &smeConfig);
    }
    else if (strncmp(command, "SETDWELLTIME PASSIVE MIN", 24) == 0)
    {
        value = value + 25;
        temp = kstrtou32(value, 10, &val);
        if (temp != 0 || val < CFG_PASSIVE_MIN_CHANNEL_TIME_MIN ||
                         val > CFG_PASSIVE_MIN_CHANNEL_TIME_MAX )
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s: argument passed for SETDWELLTIME PASSIVE MIN is incorrect", __func__);
            ret = -EFAULT;
            return ret;
        }
        pCfg->nPassiveMinChnTime = val;
        smeConfig.csrConfig.nPassiveMinChnTime = val;
        sme_UpdateConfig(hHal, &smeConfig);
    }
    else if (strncmp(command, "SETDWELLTIME", 12) == 0)
    {
        value = value + 13;
        temp = kstrtou32(value, 10, &val);
        if (temp != 0 || val < CFG_ACTIVE_MAX_CHANNEL_TIME_MIN ||
                         val > CFG_ACTIVE_MAX_CHANNEL_TIME_MAX )
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s: argument passed for SETDWELLTIME is incorrect", __func__);
            ret = -EFAULT;
            return ret;
        }
        pCfg->nActiveMaxChnTime = val;
        smeConfig.csrConfig.nActiveMaxChnTime = val;
        sme_UpdateConfig(hHal, &smeConfig);
    }
    else
    {
        ret = -EINVAL;
    }

    return ret;
}
/**
 * hdd_indicate_mgmt_frame() - Wrapper to indicate management frame to
 * user space
 * @frame_ind: Management frame data to be informed.
 *
 * This function is used to indicate management frame to
 * user space
 *
 * Return: None
 *
 */
void hdd_indicate_mgmt_frame(tSirSmeMgmtFrameInd *frame_ind)
{
	hdd_context_t *hdd_ctx;
	hdd_adapter_t *adapter;
	v_CONTEXT_t vos_context;
	int i;

	/* Get the global VOSS context.*/
	vos_context = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
	if (!vos_context) {
		hddLog(LOGE, FL("Global VOS context is Null"));
		return;
	}
	/* Get the HDD context.*/
	hdd_ctx =
	  (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, vos_context);

	if (0 != wlan_hdd_validate_context(hdd_ctx))
		return;

	if (HDD_SESSION_ID_ANY == frame_ind->sessionId) {
		for (i = 0; i < HDD_SESSION_MAX; i++) {
			adapter =
				hdd_get_adapter_by_sme_session_id(hdd_ctx, i);
			if (adapter)
				break;
		}
	} else {
		adapter = hdd_get_adapter_by_sme_session_id(hdd_ctx,
					frame_ind->sessionId);
	}

	if ((NULL != adapter) &&
		(WLAN_HDD_ADAPTER_MAGIC == adapter->magic))
		__hdd_indicate_mgmt_frame(adapter,
						frame_ind->frame_len,
						frame_ind->frameBuf,
						frame_ind->frameType,
						frame_ind->rxChan,
						frame_ind->rxRssi);
	return;
}

static void hdd_GetLink_statusCB(v_U8_t status, void *pContext)
{
   struct statsContext *pLinkContext;
   hdd_adapter_t *pAdapter;

   if (NULL == pContext) {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Bad pContext [%p]",
              __func__, pContext);
      return;
   }

   pLinkContext = pContext;
   pAdapter     = pLinkContext->pAdapter;

   spin_lock(&hdd_context_lock);

   if ((NULL == pAdapter) || (LINK_STATUS_MAGIC != pLinkContext->magic)) {
      /* the caller presumably timed out so there is nothing we can do */
      spin_unlock(&hdd_context_lock);
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: Invalid context, pAdapter [%p] magic [%08x]",
              __func__, pAdapter, pLinkContext->magic);
      return;
   }

   /* context is valid so caller is still waiting */

   /* paranoia: invalidate the magic */
   pLinkContext->magic = 0;

   /* copy over the status */
   pAdapter->linkStatus = status;

   /* notify the caller */
   complete(&pLinkContext->completion);

   /* serialization is complete */
   spin_unlock(&hdd_context_lock);
}

/**
 * hdd_get_fw_state_cb() - validates the context and notifies the caller
 * @callback_context: caller context
 *
 * Return: none
 */
static void hdd_get_fw_state_cb(void *callback_context)
{
	struct statsContext *context;
	hdd_adapter_t *adapter;

	if (NULL == callback_context) {
		hddLog(LOGE, FL("Bad pContext [%p]"), callback_context);
		return;
	}

	context = callback_context;
	adapter = context->pAdapter;

	spin_lock(&hdd_context_lock);

	if ((NULL == adapter) || (FW_STATUS_MAGIC != context->magic)) {
		/* the caller presumably timed out so there is
		 * nothing we can do
		 */
		spin_unlock(&hdd_context_lock);
		hddLog(LOGE, FL("Invalid context, Adapter [%p] magic [%08x]"),
			adapter, context->magic);
		return;
	}

	/* context is valid so caller is still waiting */

	/* paranoia: invalidate the magic */
	context->magic = 0;

	/* notify the caller */
	complete(&context->completion);

	/* serialization is complete */
	spin_unlock(&hdd_context_lock);
}

/**
 * wlan_hdd_get_fw_state() - get firmware state
 * @adapter:     pointer to the adapter
 *
 * This function sends a request to firmware and waits
 * on a timer to invoke the callback. if the callback is invoked then
 * true will be returned or otherwise fail status will be returned.
 *
 * Return: true, firmware is active.
 *         false, firmware is in bad state.
 */
bool wlan_hdd_get_fw_state(hdd_adapter_t *adapter)
{
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct statsContext context;
	eHalStatus hstatus = eHAL_STATUS_SUCCESS;
	unsigned long rc;
	bool fw_active = true;

	if (wlan_hdd_validate_context(hdd_ctx) != 0) {
		hddLog(LOGE, FL("HDD context is not valid!!!"));
		return false;
	}

	init_completion(&context.completion);
	context.pAdapter = adapter;
	context.magic = FW_STATUS_MAGIC;
	hstatus = sme_get_fw_state(WLAN_HDD_GET_HAL_CTX(adapter),
				   hdd_get_fw_state_cb,
				   &context);

	if (eHAL_STATUS_SUCCESS != hstatus) {
		hddLog(LOGE, FL("Unable to retrieve firmware status"));
		fw_active = false;
	} else {
		/* request is sent -- wait for the response */
		rc = wait_for_completion_timeout(&context.completion,
			msecs_to_jiffies(WLAN_WAIT_TIME_LINK_STATUS));
		if (!rc) {
			hddLog(LOGE,
				FL("SME timed out while retrieving firmware status"));
			fw_active = false;
		}
	}

	spin_lock(&hdd_context_lock);
	context.magic = 0;
	spin_unlock(&hdd_context_lock);

	return fw_active;
}

/**
 * wlan_hdd_get_link_status() - get link status
 * @pAdapter:     pointer to the adapter
 *
 * This function sends a request to query the link status and waits
 * on a timer to invoke the callback. if the callback is invoked then
 * latest link status shall be returned or otherwise cached value
 * will be returned.
 *
 * Return: On success, link status shall be returned.
 *         On error or not associated, link status 0 will be returned.
 */
static int wlan_hdd_get_link_status(hdd_adapter_t *pAdapter)
{
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
   struct statsContext context;
   eHalStatus hstatus;
   unsigned long rc;

   if (pHddCtx->isLogpInProgress) {
       hddLog(LOGW, FL("LOGP in Progress. Ignore!!!"));
       return 0;
   }

   if (eConnectionState_Associated != pHddStaCtx->conn_info.connState) {
       /* If not associated, then expected link status return value is 0 */
       hddLog(LOG1, FL("Not associated!"));
       return 0;
   }

   init_completion(&context.completion);
   context.pAdapter = pAdapter;
   context.magic = LINK_STATUS_MAGIC;
   hstatus = sme_getLinkStatus(WLAN_HDD_GET_HAL_CTX(pAdapter),
                               hdd_GetLink_statusCB,
                               &context,
                               pAdapter->sessionId);
   if (eHAL_STATUS_SUCCESS != hstatus) {
       hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: Unable to retrieve link status", __func__);
       /* return a cached value */
   } else {
       /* request is sent -- wait for the response */
       rc = wait_for_completion_timeout(&context.completion,
                                msecs_to_jiffies(WLAN_WAIT_TIME_LINK_STATUS));
       if (!rc) {
          hddLog(VOS_TRACE_LEVEL_ERROR,
              FL("SME timed out while retrieving link status"));
      }
   }

   spin_lock(&hdd_context_lock);
   context.magic = 0;
   spin_unlock(&hdd_context_lock);

   /* either callback updated pAdapter stats or it has cached data */
   return  pAdapter->linkStatus;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void hdd_wma_send_fastreassoc_cmd(int sessionId, tSirMacAddr bssid,
                                     int channel)
{
    t_wma_roam_invoke_cmd *fastreassoc;
    vos_msg_t msg = {0};

    fastreassoc = vos_mem_malloc(sizeof(*fastreassoc));
    if (NULL == fastreassoc) {
        hddLog(LOGE, FL("vos_mem_alloc failed for fastreassoc"));
        return;
    }
    fastreassoc->vdev_id = sessionId;
    fastreassoc->channel = channel;
    fastreassoc->bssid[0] = bssid[0];
    fastreassoc->bssid[1] = bssid[1];
    fastreassoc->bssid[2] = bssid[2];
    fastreassoc->bssid[3] = bssid[3];
    fastreassoc->bssid[4] = bssid[4];
    fastreassoc->bssid[5] = bssid[5];

    msg.type = SIR_HAL_ROAM_INVOKE;
    msg.reserved = 0;
    msg.bodyptr = fastreassoc;
    if (VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA,
                                                             &msg)) {
        vos_mem_free(fastreassoc);
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               FL("Not able to post ROAM_INVOKE_CMD message to WMA"));
    }
}
#endif

/**
 * hdd_set_miracast_mode() - function used to set the miracast mode value
 * @pAdapter: pointer to the adapter of the interface.
 * @command: pointer to the command buffer "MIRACAST <value>".
 * Return: 0 on success -EINVAL on failure.
 */
int hdd_set_miracast_mode(hdd_adapter_t *pAdapter, tANI_U8 *command)
{
    tHalHandle hHal;
    tANI_U8 filterType = 0;
    hdd_context_t *pHddCtx = NULL;
    tANI_U8 *value;
    int ret;
    eHalStatus ret_status;

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    if (0 != wlan_hdd_validate_context(pHddCtx)) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("pHddCtx is not valid, Unable to set miracast mode"));
        return -EINVAL;
    }

    hHal = pHddCtx->hHal;

    value = command + 9;

    /* Convert the value from ascii to integer */
    ret = kstrtou8(value, 10, &filterType);
    if (ret < 0) {
        /* If the input value is greater than max value of datatype,
         * then also kstrtou8 fails
         */
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: kstrtou8 failed range", __func__);
        return -EINVAL;
    }

    /* Filtertype value should be either 0-Disabled, 1-Source, 2-sink */
    if ((filterType < WLAN_HDD_DRIVER_MIRACAST_CFG_MIN_VAL ) ||
            (filterType > WLAN_HDD_DRIVER_MIRACAST_CFG_MAX_VAL)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Accepted Values are 0 to 2."
                "0-Disabled, 1-Source, 2-Sink", __func__);
        return -EINVAL;
    }
    hddLog(VOS_TRACE_LEVEL_INFO, "%s: miracast mode %hu", __func__, filterType);
    pHddCtx->miracast_value = filterType;

    ret_status = sme_set_miracast(hHal, filterType);
    if (eHAL_STATUS_SUCCESS != ret_status) {
        hddLog(LOGE, "Failed to set miracast");
        return -EBUSY;
    }

    if (hdd_is_mcc_in_24G(pHddCtx)) {
        return hdd_set_mas(pAdapter, filterType);
    }

    return 0;
}

/* Function header is left blank intentionally */
static int hdd_parse_set_ibss_oui_data_command(uint8_t *command, uint8_t *ie,
                                             int32_t *oui_length, int32_t limit)
{
   uint8_t len;
   uint8_t data;

   while ((SPACE_ASCII_VALUE == *command) && ('\0' != *command)) {
      command++;
      limit--;
   }

   len = 2;

   while ((SPACE_ASCII_VALUE != *command) && ('\0' != *command) &&
          (limit > 1)) {
      sscanf(command, "%02x", (unsigned int *)&data);
      ie[len++] = data;
      command += 2;
      limit -= 2;
   }

   *oui_length = len - 2;

   while ((SPACE_ASCII_VALUE == *command) && ('\0' != *command)) {
      command++;
      limit--;
   }

   while ((SPACE_ASCII_VALUE != *command) && ('\0' != *command) &&
         (limit > 1)) {
      sscanf(command, "%02x", (unsigned int *)&data);
      ie[len++] = data;
      command += 2;
      limit -= 2;
   }

   ie[0] = IE_EID_VENDOR;
   ie[1] = len - 2;

   return len;
}


/**
 * hdd_set_mas() - Function to set MAS value to UMAC
 * @adapter:		Pointer to HDD adapter
 * @mas_value:		0-Disable, 1-Enable MAS
 *
 * This function passes down the value of MAS to UMAC
 *
 * Return: Configuration message posting status, SUCCESS or Fail
 *
 */
int32_t hdd_set_mas(hdd_adapter_t *adapter, tANI_U8 mas_value)
{
	hdd_context_t *hdd_ctx = NULL;
	eHalStatus ret_status;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx)
		return -EFAULT;

	if (mas_value) {
		/* Miracast is ON. Disable MAS and configure P2P quota */
		if (hdd_ctx->cfg_ini->enableMCCAdaptiveScheduler) {
			if (cfgSetInt(hdd_ctx->hHal,
				WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED, 0)
				!= eSIR_SUCCESS) {
				hddLog(LOGE,
					"Could not pass on WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED to CCM");
			}

			ret_status = sme_set_mas(false);
			if (eHAL_STATUS_SUCCESS != ret_status) {
				hddLog(LOGE, "Failed to disable MAS");
				return -EBUSY;
			}
		}

		/* Config p2p quota */
		if (adapter->device_mode == WLAN_HDD_INFRA_STATION)
			hdd_wlan_set_mcc_p2p_quota(adapter,
				100 - HDD_DEFAULT_MCC_P2P_QUOTA);
		else if (adapter->device_mode == WLAN_HDD_P2P_GO)
			hdd_wlan_go_set_mcc_p2p_quota(adapter,
				HDD_DEFAULT_MCC_P2P_QUOTA);
		else
			hdd_wlan_set_mcc_p2p_quota(adapter,
				HDD_DEFAULT_MCC_P2P_QUOTA);
	} else {
		/* Reset p2p quota */
		if (adapter->device_mode == WLAN_HDD_P2P_GO)
			hdd_wlan_go_set_mcc_p2p_quota(adapter,
						HDD_RESET_MCC_P2P_QUOTA);
		else
			hdd_wlan_set_mcc_p2p_quota(adapter,
						HDD_RESET_MCC_P2P_QUOTA);

		/* Miracast is OFF. Enable MAS and reset P2P quota */
		if (hdd_ctx->cfg_ini->enableMCCAdaptiveScheduler) {
			if (cfgSetInt(hdd_ctx->hHal,
				WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED, 1)
				!= eSIR_SUCCESS) {
				hddLog(LOGE, "Could not pass on WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED to CCM");
			}

			/* Enable MAS */
			ret_status = sme_set_mas(true);
			if (eHAL_STATUS_SUCCESS != ret_status) {
				hddLog(LOGE, "Unable to enable MAS");
				return -EBUSY;
			}
		}
	}

	return 0;
}

/**
 * hdd_is_mcc_in_24G() - Function to check for MCC in 2.4GHz
 * @hdd_ctx:	Pointer to HDD context
 *
 * This function is used to check for MCC operation in 2.4GHz band.
 * STA, P2P and SAP adapters are only considered.
 *
 * Return: Non zero value if MCC is detected in 2.4GHz band
 *
 */
uint8_t hdd_is_mcc_in_24G(hdd_context_t *hdd_ctx)
{
	VOS_STATUS status;
	hdd_adapter_t *hdd_adapter = NULL;
	hdd_adapter_list_node_t *adapter_node = NULL, *next = NULL;
	uint8_t ret = 0;
	hdd_station_ctx_t *sta_ctx;
	hdd_ap_ctx_t *ap_ctx;
	uint8_t ch1 = 0, ch2 = 0;
	uint8_t channel = 0;
	hdd_hostapd_state_t *hostapd_state;

	status =  hdd_get_front_adapter(hdd_ctx, &adapter_node);

	/* loop through all adapters and check MCC for STA,P2P,SAP adapters */
	while (NULL != adapter_node && VOS_STATUS_SUCCESS == status) {
		hdd_adapter = adapter_node->pAdapter;

		if (!((hdd_adapter->device_mode >= WLAN_HDD_INFRA_STATION)
					|| (hdd_adapter->device_mode
						<= WLAN_HDD_P2P_GO))) {
			/* skip for other adapters */
			status = hdd_get_next_adapter(hdd_ctx,
					adapter_node, &next);
			adapter_node = next;
			continue;
		} else {
			if (WLAN_HDD_INFRA_STATION ==
					hdd_adapter->device_mode ||
					WLAN_HDD_P2P_CLIENT ==
					hdd_adapter->device_mode) {
				sta_ctx =
					WLAN_HDD_GET_STATION_CTX_PTR(
								hdd_adapter);
				if (eConnectionState_Associated ==
						sta_ctx->conn_info.connState)
					channel =
						sta_ctx->conn_info.
							operationChannel;
			} else if (WLAN_HDD_P2P_GO ==
					hdd_adapter->device_mode ||
					WLAN_HDD_SOFTAP ==
					hdd_adapter->device_mode) {
				ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(hdd_adapter);
				hostapd_state =
					WLAN_HDD_GET_HOSTAP_STATE_PTR(
								hdd_adapter);
				if (hostapd_state->bssState == BSS_START &&
						hostapd_state->vosStatus ==
						VOS_STATUS_SUCCESS)
					channel = ap_ctx->operatingChannel;
			}

			if ((ch1 == 0) ||
					((ch2 != 0) && (ch2 != channel))) {
				ch1 = channel;
			} else if ((ch2 == 0) ||
					((ch1 != 0) && (ch1 != channel))) {
				ch2 = channel;
			}

			if ((ch1 != 0 && ch2 != 0) && (ch1 != ch2) &&
					((ch1 <= SIR_11B_CHANNEL_END) &&
					 (ch2 <= SIR_11B_CHANNEL_END))) {
				hddLog(LOGE,
					"MCC in 2.4Ghz on channels %d and %d",
					ch1, ch2);
				return 1;
			}

			status = hdd_get_next_adapter(hdd_ctx,
					adapter_node, &next);
			adapter_node = next;
		}
	}
	return ret;
}

/**
 * wlan_hdd_get_link_speed() - get link speed
 * @pAdapter:     pointer to the adapter
 * @link_speed:   pointer to link speed
 *
 * This function fetches per bssid link speed.
 *
 * Return: if associated, link speed shall be returned.
 *         if not associated, link speed of 0 is returned.
 *         On error, error number will be returned.
 */
int wlan_hdd_get_link_speed(hdd_adapter_t *sta_adapter, uint32_t *link_speed)
{
    hdd_context_t *hddctx = WLAN_HDD_GET_CTX(sta_adapter);
    hdd_station_ctx_t *hdd_stactx = WLAN_HDD_GET_STATION_CTX_PTR(sta_adapter);
    int ret;

    ret = wlan_hdd_validate_context(hddctx);

    if (0 != ret) {
        hddLog(LOGE, FL("HDD context is not valid"));
        return ret;
    }

    if (eConnectionState_Associated != hdd_stactx->conn_info.connState) {
       /* we are not connected so we don't have a classAstats */
       *link_speed = 0;
    } else {
        VOS_STATUS status;
        tSirMacAddr bssid;

        vos_mem_copy(bssid, hdd_stactx->conn_info.bssId, VOS_MAC_ADDR_SIZE);

        status = wlan_hdd_get_linkspeed_for_peermac(sta_adapter, bssid);
        if (!VOS_IS_STATUS_SUCCESS(status)) {
           hddLog(LOGE, FL("Unable to retrieve SME linkspeed"));
           return -EINVAL;
        }
        *link_speed = sta_adapter->ls_stats.estLinkSpeed;
        /* linkspeed in units of 500 kbps */
        *link_speed = (*link_speed) / 500;
    }
    return 0;
}

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
/**
 * hdd_parse_ese_beacon_req() - Parse ese beacon request
 * @pValue:     Pointer to input data (its a NUL terminated string)
 * @pEseBcnReq: pEseBcnReq output pointer to store parsed ie information
 *
 * This function parses the ese beacon request passed in the format
 * CCXBEACONREQ<space><Number of fields><space><Measurement token>
 * <space>Channel 1<space>Scan Mode <space>Meas Duration<space>Channel N
 * <space>Scan Mode N<space>Meas Duration N
 * if the Number of bcn req fields (N) does not match with the actual number of
 * fields passed then take N.
 * <Meas Token><Channel><Scan Mode> and <Meas Duration> are treated as one pair
 * For example, CCXBEACONREQ 2 1 1 1 30 2 44 0 40.
 * This function does not take care of removing duplicate channels from the list
 *
 * Return: 0 for success non-zero for failure
 */
static VOS_STATUS hdd_parse_ese_beacon_req(tANI_U8 *pValue,
                                     tCsrEseBeaconReq *pEseBcnReq)
{
    tANI_U8 *inPtr = pValue;
    int tempInt = 0;
    int j = 0, i = 0, v = 0;
    char buf[32];

    inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
    /* no argument after the command */
    if (NULL == inPtr) {
        return -EINVAL;
    } else if (SPACE_ASCII_VALUE != *inPtr) {
        /* no space after the command */
        return -EINVAL;
    }

    /* removing empty spaces */
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr)) inPtr++;

    /* no argument followed by spaces */
    if ('\0' == *inPtr) return -EINVAL;

    /* Getting the first argument ie measurement token */
    v = sscanf(inPtr, "%31s ", buf);
    if (1 != v) return -EINVAL;

    v = kstrtos32(buf, 10, &tempInt);
    if (v < 0) return -EINVAL;

    pEseBcnReq->numBcnReqIe = tempInt;

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
               "Number of Bcn Req Ie fields(%d)", pEseBcnReq->numBcnReqIe);

    for (j = 0; j < (pEseBcnReq->numBcnReqIe); j++) {
        for (i = 0; i < 4; i++) {
            /* inPtr pointing to the beginning of first space after number of
               ie fields */
            inPtr = strpbrk( inPtr, " " );
            /* no ie data after the number of ie fields argument */
            if (NULL == inPtr) return -EINVAL;

            /* removing empty space */
            while ((SPACE_ASCII_VALUE == *inPtr) && ('\0' != *inPtr)) inPtr++;

            /* no ie data after the number of ie fields argument and spaces */
            if ('\0' == *inPtr) return -EINVAL;

            v = sscanf(inPtr, "%31s ", buf);
            if (1 != v) return -EINVAL;

            v = kstrtos32(buf, 10, &tempInt);
            if (v < 0) return -EINVAL;

            switch (i) {
            case 0:  /* Measurement token */
                if (tempInt <= 0) {
                   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             "Invalid Measurement Token(%d)", tempInt);
                   return -EINVAL;
                }
                pEseBcnReq->bcnReq[j].measurementToken = tempInt;
                break;

            case 1:  /* Channel number */
                if ((tempInt <= 0) ||
                    (tempInt > WNI_CFG_CURRENT_CHANNEL_STAMAX)) {
                   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             "Invalid Channel Number(%d)", tempInt);
                   return -EINVAL;
                }
                pEseBcnReq->bcnReq[j].channel = tempInt;
                break;

            case 2:  /* Scan mode */
                if ((tempInt < eSIR_PASSIVE_SCAN) ||
                    (tempInt > eSIR_BEACON_TABLE)) {
                   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             "Invalid Scan Mode(%d) Expected{0|1|2}", tempInt);
                   return -EINVAL;
                }
                pEseBcnReq->bcnReq[j].scanMode= tempInt;
                break;

            case 3:  /* Measurement duration */
                if (((tempInt <= 0) &&
                    (pEseBcnReq->bcnReq[j].scanMode != eSIR_BEACON_TABLE)) ||
                    ((tempInt < 0) &&
                    (pEseBcnReq->bcnReq[j].scanMode == eSIR_BEACON_TABLE))) {
                   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             "Invalid Measurement Duration(%d)", tempInt);
                   return -EINVAL;
                }
                pEseBcnReq->bcnReq[j].measurementDuration = tempInt;
                break;
            }
        }
    }

    for (j = 0; j < pEseBcnReq->numBcnReqIe; j++) {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "Index(%d) Measurement Token(%u) Channel(%u) Scan Mode(%u) Measurement Duration(%u)",
                   j,
                   pEseBcnReq->bcnReq[j].measurementToken,
                   pEseBcnReq->bcnReq[j].channel,
                   pEseBcnReq->bcnReq[j].scanMode,
                   pEseBcnReq->bcnReq[j].measurementDuration);
    }

    return VOS_STATUS_SUCCESS;
}

static void hdd_GetTsmStatsCB( tAniTrafStrmMetrics tsmMetrics,
                               const tANI_U32 staId,
                               void *pContext )
{
   struct statsContext *pStatsContext = NULL;
   hdd_adapter_t       *pAdapter = NULL;

   if (NULL == pContext) {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Bad param, pContext [%p]",
             __func__, pContext);
      return;
   }

   /* there is a race condition that exists between this callback
      function and the caller since the caller could time out either
      before or while this code is executing.  we use a spinlock to
      serialize these actions */
   spin_lock(&hdd_context_lock);

   pStatsContext = pContext;
   pAdapter      = pStatsContext->pAdapter;
   if ((NULL == pAdapter) || (STATS_CONTEXT_MAGIC != pStatsContext->magic)) {
      /* the caller presumably timed out so there is nothing we can do */
      spin_unlock(&hdd_context_lock);
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: Invalid context, pAdapter [%p] magic [%08x]",
              __func__, pAdapter, pStatsContext->magic);
      return;
   }

   /* context is valid so caller is still waiting */

   /* paranoia: invalidate the magic */
   pStatsContext->magic = 0;

   /* copy over the tsm stats */
   pAdapter->tsmStats.UplinkPktQueueDly = tsmMetrics.UplinkPktQueueDly;
   vos_mem_copy(pAdapter->tsmStats.UplinkPktQueueDlyHist,
                 tsmMetrics.UplinkPktQueueDlyHist,
                 sizeof(pAdapter->tsmStats.UplinkPktQueueDlyHist)/
                 sizeof(pAdapter->tsmStats.UplinkPktQueueDlyHist[0]));
   pAdapter->tsmStats.UplinkPktTxDly = tsmMetrics.UplinkPktTxDly;
   pAdapter->tsmStats.UplinkPktLoss = tsmMetrics.UplinkPktLoss;
   pAdapter->tsmStats.UplinkPktCount = tsmMetrics.UplinkPktCount;
   pAdapter->tsmStats.RoamingCount = tsmMetrics.RoamingCount;
   pAdapter->tsmStats.RoamingDly = tsmMetrics.RoamingDly;

   /* notify the caller */
   complete(&pStatsContext->completion);

   /* serialization is complete */
   spin_unlock(&hdd_context_lock);
}

static VOS_STATUS hdd_get_tsm_stats(hdd_adapter_t *pAdapter,
                                    const tANI_U8 tid,
                                    tAniTrafStrmMetrics* pTsmMetrics)
{
   hdd_station_ctx_t *pHddStaCtx = NULL;
   eHalStatus         hstatus;
   VOS_STATUS         vstatus = VOS_STATUS_SUCCESS;
   unsigned long      rc;
   struct statsContext context;
   hdd_context_t     *pHddCtx = NULL;

   if (NULL == pAdapter) {
       hddLog(LOGE, FL("pAdapter is NULL"));
       return VOS_STATUS_E_FAULT;
   }

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

   /* we are connected prepare our callback context */
   init_completion(&context.completion);
   context.pAdapter = pAdapter;
   context.magic = STATS_CONTEXT_MAGIC;

   /* query tsm stats */
   hstatus = sme_GetTsmStats(pHddCtx->hHal, hdd_GetTsmStatsCB,
                         pHddStaCtx->conn_info.staId[ 0 ],
                         pHddStaCtx->conn_info.bssId,
                         &context, pHddCtx->pvosContext, tid);
   if (eHAL_STATUS_SUCCESS != hstatus) {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Unable to retrieve statistics",
             __func__);
      vstatus = VOS_STATUS_E_FAULT;
   } else {
      /* request was sent -- wait for the response */
      rc = wait_for_completion_timeout(&context.completion,
                                    msecs_to_jiffies(WLAN_WAIT_TIME_STATS));
      if (!rc) {
         hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: SME timed out while retrieving statistics",
                __func__);
         vstatus = VOS_STATUS_E_TIMEOUT;
      }
   }

   /* either we never sent a request, we sent a request and received a
      response or we sent a request and timed out.  if we never sent a
      request or if we sent a request and got a response, we want to
      clear the magic out of paranoia.  if we timed out there is a
      race condition such that the callback function could be
      executing at the same time we are. of primary concern is if the
      callback function had already verified the "magic" but had not
      yet set the completion variable when a timeout occurred. we
      serialize these activities by invalidating the magic while
      holding a shared spinlock which will cause us to block if the
      callback is currently executing */
   spin_lock(&hdd_context_lock);
   context.magic = 0;
   spin_unlock(&hdd_context_lock);

   if (VOS_STATUS_SUCCESS == vstatus) {
      pTsmMetrics->UplinkPktQueueDly = pAdapter->tsmStats.UplinkPktQueueDly;
      vos_mem_copy(pTsmMetrics->UplinkPktQueueDlyHist,
                   pAdapter->tsmStats.UplinkPktQueueDlyHist,
                   sizeof(pAdapter->tsmStats.UplinkPktQueueDlyHist)/
                   sizeof(pAdapter->tsmStats.UplinkPktQueueDlyHist[0]));
      pTsmMetrics->UplinkPktTxDly = pAdapter->tsmStats.UplinkPktTxDly;
      pTsmMetrics->UplinkPktLoss = pAdapter->tsmStats.UplinkPktLoss;
      pTsmMetrics->UplinkPktCount = pAdapter->tsmStats.UplinkPktCount;
      pTsmMetrics->RoamingCount = pAdapter->tsmStats.RoamingCount;
      pTsmMetrics->RoamingDly = pAdapter->tsmStats.RoamingDly;
   }
   return vstatus;
}

/**
 * hdd_parse_get_cckm_ie() - HDD Parse and fetch the CCKM IE
 * @pValue:  Pointer to input data
 * @pCckmIe: Pointer to output cckm Ie
 * @pCckmIeLen: Pointer to output cckm ie length
 *
 * This function parses the SETCCKM IE command
 *
 * Return: 0 for success non-zero for failure
 */
static VOS_STATUS
hdd_parse_get_cckm_ie(tANI_U8 *pValue, tANI_U8 **pCckmIe, tANI_U8 *pCckmIeLen)
{
    tANI_U8 *inPtr = pValue;
    tANI_U8 *dataEnd;
    int      j = 0;
    int      i = 0;
    tANI_U8  tempByte = 0;
    inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
    /* no argument after the command */
    if (NULL == inPtr) {
        return -EINVAL;
    } else if (SPACE_ASCII_VALUE != *inPtr) {
        /* no space after the command */
        return -EINVAL;
    }
    /* removing empty spaces */
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;
    /* no argument followed by spaces */
    if ('\0' == *inPtr) {
        return -EINVAL;
    }
    /* find the length of data */
    dataEnd = inPtr;
    while('\0' !=  *dataEnd) {
        dataEnd++;
        ++(*pCckmIeLen);
    }
    if (*pCckmIeLen <= 0)  return -EINVAL;
    /*
     * Allocate the number of bytes based on the number of input characters
     * whether it is even or odd.
     * if the number of input characters are even, then we need N/2 byte.
     * if the number of input characters are odd, then we need do (N+1)/2 to
     * compensate rounding off.
     * For example, if N = 18, then (18 + 1)/2 = 9 bytes are enough.
     * If N = 19, then we need 10 bytes, hence (19 + 1)/2 = 10 bytes
     */
    *pCckmIe = vos_mem_malloc((*pCckmIeLen + 1)/2);
    if (NULL == *pCckmIe) {
        hddLog(LOGP, FL("vos_mem_alloc failed"));
        return -ENOMEM;
    }
    vos_mem_zero(*pCckmIe, (*pCckmIeLen + 1)/2);
    /*
     * the buffer received from the upper layer is character buffer,
     * we need to prepare the buffer taking 2 characters in to a U8 hex
     * decimal number for example 7f0000f0...form a buffer to contain
     * 7f in 0th location, 00 in 1st and f0 in 3rd location
     */
    for (i = 0, j = 0; j < *pCckmIeLen; j += 2) {
        tempByte = (hdd_parse_hex(inPtr[j]) << 4)
                   | (hdd_parse_hex(inPtr[j + 1]));
        (*pCckmIe)[i++] = tempByte;
    }
    *pCckmIeLen = i;
    return VOS_STATUS_SUCCESS;
}

#endif /*FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */

/**
 * drv_cmd_set_fcc_channel() - handle fcc constraint request
 * @hdd_ctx: HDD context
 * @cmd: command ptr
 * @cmd_len: command len
 *
 * Return: status
 */
static int drv_cmd_set_fcc_channel(hdd_context_t *hdd_ctx, uint8_t *cmd,
                                   uint8_t cmd_len)
{
	uint8_t *value;
	uint8_t fcc_constraint;
	eHalStatus status;
	int ret = 0;

	value =  cmd + cmd_len + 1;

	ret = kstrtou8(value, 10, &fcc_constraint);
	if ((ret < 0) || (fcc_constraint > 1)) {
		/*
		 *  If the input value is greater than max value of datatype,
		 *  then also it is a failure
		 */
		hddLog(LOGE, FL("value out of range"));
		return -EINVAL;
	}

	status = sme_handle_set_fcc_channel(hdd_ctx->hHal, !fcc_constraint);
	if (status != eHAL_STATUS_SUCCESS)
		ret = -EPERM;

	return ret;
}

/**
 * hdd_set_rx_filter() - set RX filter
 * @adapter: Pointer to adapter
 * @action: Filter action
 * @pattern: Address pattern
 *
 * Address pattern is most significant byte of address for example
 * 0x01 for IPV4 multicast address
 * 0x33 for IPV6 multicast address
 * 0xFF for broadcast address
 *
 * Return: 0 for success, non-zero for failure
 */
static int hdd_set_rx_filter(hdd_adapter_t *adapter, bool action,
			uint8_t pattern)
{
	int ret;
	uint8_t i;
	tHalHandle handle;
	tSirRcvFltMcAddrList *filter;
	hdd_context_t* hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	handle = hdd_ctx->hHal;

	if (NULL == handle) {
		hddLog(LOGE, FL("HAL Handle is NULL"));
		return -EINVAL;
	}

	/*
	 * If action is false it means start dropping packets
	 * Set addr_filter_pattern which will be used when sending
	 * MC/BC address list to target
	 */
	if (!action)
		adapter->addr_filter_pattern = pattern;
	else
		adapter->addr_filter_pattern = 0;

	if (((adapter->device_mode == WLAN_HDD_INFRA_STATION) ||
		(adapter->device_mode == WLAN_HDD_P2P_CLIENT)) &&
		adapter->mc_addr_list.mc_cnt &&
		hdd_connIsConnected(WLAN_HDD_GET_STATION_CTX_PTR(adapter))) {


		filter = vos_mem_malloc(sizeof(*filter));
		if (NULL == filter) {
			hddLog(LOGE, FL("Could not allocate Memory"));
			return -ENOMEM;
		}
		vos_mem_zero(filter, sizeof(*filter));
		filter->action = action;
		for (i = 0; i < adapter->mc_addr_list.mc_cnt; i++) {
			if (!memcmp(adapter->mc_addr_list.addr[i],
				&pattern, 1)) {
				memcpy(filter->multicastAddr[i],
					adapter->mc_addr_list.addr[i],
					sizeof(adapter->mc_addr_list.addr[i]));
				filter->ulMulticastAddrCnt++;
				hddLog(LOG1, "%s RX filter : addr ="
				    MAC_ADDRESS_STR,
				    action ? "setting" : "clearing",
				    MAC_ADDR_ARRAY(filter->multicastAddr[i]));
			}
		}
		/* Set rx filter */
		sme_8023MulticastList(handle, adapter->sessionId, filter);
		vos_mem_free(filter);
	} else {
		hddLog(LOGW, FL("mode %d mc_cnt %d"),
			adapter->device_mode, adapter->mc_addr_list.mc_cnt);
	}

	return 0;
}

/**
 * hdd_driver_rxfilter_comand_handler() - RXFILTER driver command handler
 * @command: Pointer to input string driver command
 * @adapter: Pointer to adapter
 * @action: Action to enable/disable filtering
 *
 * If action == false
 * Start filtering out data packets based on type
 * RXFILTER-REMOVE 0 -> Start filtering out unicast data packets
 * RXFILTER-REMOVE 1 -> Start filtering out broadcast data packets
 * RXFILTER-REMOVE 2 -> Start filtering out IPV4 mcast data packets
 * RXFILTER-REMOVE 3 -> Start filtering out IPV6 mcast data packets
 *
 * if action == true
 * Stop filtering data packets based on type
 * RXFILTER-ADD 0 -> Stop filtering unicast data packets
 * RXFILTER-ADD 1 -> Stop filtering broadcast data packets
 * RXFILTER-ADD 2 -> Stop filtering IPV4 mcast data packets
 * RXFILTER-ADD 3 -> Stop filtering IPV6 mcast data packets
 *
 * Current implementation only supports IPV4 address filtering by
 * selectively allowing IPV4 multicast data packest based on
 * address list received in .ndo_set_rx_mode
 *
 * Return: 0 for success, non-zero for failure
 */
static int hdd_driver_rxfilter_comand_handler(uint8_t *command,
						hdd_adapter_t *adapter,
						bool action)
{
	int ret = 0;
	uint8_t *value;
	uint8_t type;

	value = command;
	/* Skip space after RXFILTER-REMOVE OR RXFILTER-ADD based on action */
	if (!action)
		value = command + 16;
	else
		value = command + 13;
	ret = kstrtou8(value, 10, &type);
	if (ret < 0) {
		hddLog(LOGE,
			FL("kstrtou8 failed invalid input value %d"), type);
		return -EINVAL;
	}

	switch (type) {
	case 2:
		/* Set rx filter for IPV4 multicast data packets */
		ret = hdd_set_rx_filter(adapter, action, 0x01);
		break;
	default:
		hddLog(LOG1, FL("Unsupported RXFILTER type %d"), type);
		break;
	}

	return ret;
}

/**
 * hdd_parse_setantennamode_command() - HDD Parse SETANTENNAMODE
 * command
 * @value: Pointer to SETANTENNAMODE command
 * @mode: Pointer to antenna mode
 *
 * This function parses the SETANTENNAMODE command passed in the format
 * SETANTENNAMODE<space>mode
 *
 * Return: 0 for success non-zero for failure
 */
static int hdd_parse_setantennamode_command(const uint8_t *value,
					int *mode)
{
	const uint8_t *in_ptr = value;
	int tmp, v;
	char arg1[32];
	*mode = 0;

	in_ptr = strnchr(value, strlen(value), SPACE_ASCII_VALUE);

	/* no argument after the command */
	if (NULL == in_ptr) {
		hddLog(LOGE, FL("No argument after the command"));
		return -EINVAL;
	}

	/* no space after the command */
	if (SPACE_ASCII_VALUE != *in_ptr) {
		hddLog(LOGE, FL("No space after the command"));
		return -EINVAL;
	}

	/* remove empty spaces */
	while ((SPACE_ASCII_VALUE == *in_ptr) && ('\0' != *in_ptr))
		in_ptr++;

	/* no argument followed by spaces */
	if ('\0' == *in_ptr) {
		hddLog(LOGE, FL("No argument followed by spaces"));
		return -EINVAL;
	}

	/* get the argument i.e. antenna mode */
	v = sscanf(in_ptr, "%31s ", arg1);
	if (1 != v) {
		hddLog(LOGE, FL("argument retrieval from cmd string failed"));
		return -EINVAL;
	}

	v = kstrtos32(arg1, 10, &tmp);
	if (v < 0) {
		hddLog(LOGE, FL("argument string to integer conversion failed"));
		return -EINVAL;
	}
	*mode = tmp;

	return 0;
}

/**
 * hdd_is_supported_chain_mask_2x2() - Verify if supported chain
 * mask is 2x2 mode
 * @hdd_ctx: Pointer to hdd contex
 *
 * Return: true if supported chain mask 2x2 else false
 */
static bool hdd_is_supported_chain_mask_2x2(hdd_context_t *hdd_ctx)
{
	hdd_config_t *config = hdd_ctx->cfg_ini;

	if (hdd_ctx->per_band_chainmask_supp == 0x01) {
		return (((hdd_ctx->supp_2g_chain_mask & 0x03)
			 == 0x03) ||
			((hdd_ctx->supp_5g_chain_mask & 0x03)
			 == 0x03)) ? true : false;
	}

	return (config->enable2x2 == 0x01) ? true : false;
}

/**
 * hdd_is_supported_chain_mask_1x1() - Verify if the supported
 * chain mask is 1x1
 * @hdd_ctx: Pointer to hdd contex
 *
 * Return: true if supported chain mask 1x1 else false
 */
static bool hdd_is_supported_chain_mask_1x1(hdd_context_t *hdd_ctx)
{
	hdd_config_t *config = hdd_ctx->cfg_ini;

	if (hdd_ctx->per_band_chainmask_supp == 0x01) {
		return ((hdd_ctx->supp_2g_chain_mask <= 0x02) &&
			(hdd_ctx->supp_5g_chain_mask <= 0x02)) ?
			true : false;
	}

	return (!config->enable2x2) ? true : false;
}

/**
 * switch_antenna_mode_non_conn_state() - Dynamic switch to 1x1
 * antenna mode when there are no connections
 * @hdd_ctx: Pointer to hdd contex
 * @adapter: Pointer to hdd adapter
 * @chains: Number of TX/RX chains to set
 *
 * Return: 0 if success else non zero
 */
static int switch_antenna_mode_non_conn_state(hdd_context_t *hdd_ctx,
					      hdd_adapter_t *adapter,
					      uint8_t chains)
{
	int ret;
	eHalStatus hal_status;
	bool enable_smps;
	int smps_mode;

	ret = wlan_hdd_update_txrx_chain_mask(hdd_ctx,
					(chains == 2) ? 0x3 : 0x1);

	if (0 != ret) {
		hddLog(LOGE,
		       FL("Failed to update chain mask: %d"),
		       chains);
		return ret;
	}

	/* Update HT SMPS as static/disabled in the SME configuration
	 * If there is STA connection followed by dynamic switch
	 * to 1x1 protocol stack would include SM power save IE as
	 * static in the assoc mgmt frame and after association
	 * SMPS force mode command will be sent to FW to initiate
	 * SMPS action frames to AP. In this case, SMPS force mode
	 * command event can be expected from firmware with the
	 * TX status of SMPS action frames. Inclusion of SM power
	 * save IE and sending of SMPS action frames will not happen
	 * for switch to 2x2 mode. But SME config should still be
	 * updated to disabled.
	 */
	adapter->smps_force_mode_status = 0;

	enable_smps = (chains == 1) ? true : false;
	smps_mode = (chains == 1) ? HDD_SMPS_MODE_STATIC :
			HDD_SMPS_MODE_DISABLED;

	hal_status = sme_update_mimo_power_save(hdd_ctx->hHal,
			enable_smps, smps_mode);
	if (eHAL_STATUS_SUCCESS != hal_status) {
		hddLog(LOG1,
		       FL("Update MIMO power SME config failed: %d"),
		       hal_status);
		return -EFAULT;
	}

	hddLog(LOG1, FL("Updated SME config enable smps: %d mode: %d"),
	       enable_smps, smps_mode);

	return 0;
}

/**
 * switch_to_1x1_connected_sta_state() - Dynamic switch to 1x1
 * antenna mode in standalone station
 * @hdd_ctx: Pointer to hdd contex
 * @adapter: Pointer to hdd adapter
 *
 * Return: 0 if success else non zero
 */
static int switch_to_1x1_connected_sta_state(hdd_context_t *hdd_ctx,
					     hdd_adapter_t *adapter)
{
	int ret;
	eHalStatus hal_status;
	bool send_smps;

	/*  Check TDLS status and update antenna mode */
	ret = wlan_hdd_tdls_antenna_switch(hdd_ctx, adapter,
					   HDD_ANTENNA_MODE_1X1);
	if (0 != ret)
		return ret;

	/* If intersection of sta and AP NSS is 1x1 then
	 * skip SMPS indication to AP. Only update the chain mask
	 * and other configuration.
	 */
	send_smps = sme_is_sta_smps_allowed(hdd_ctx->hHal,
					adapter->sessionId);
	if (!send_smps) {
		hddLog(LOGE, FL("Need not indicate SMPS to AP"));
		goto chain_mask;
	}

	INIT_COMPLETION(adapter->smps_force_mode_comp_var);

	hddLog(LOG1, FL("Send SMPS force mode command"));
	ret = process_wma_set_command((int)adapter->sessionId,
				WMI_STA_SMPS_FORCE_MODE_CMDID,
				WMI_SMPS_FORCED_MODE_STATIC,
				VDEV_CMD);
	if (0 != ret) {
		hddLog(LOGE,
		       FL("Failed to send SMPS force mode to static"));
		return ret;
	}

	/* Block on SMPS force mode event only for mode static */
	ret = wait_for_completion_timeout(
		&adapter->smps_force_mode_comp_var,
		msecs_to_jiffies(WLAN_WAIT_SMPS_FORCE_MODE));
	if (!ret) {
		hddLog(LOGE,
			FL("SMPS force mode event timeout: %d"),
			ret);
		return -EFAULT;
	}
	ret = adapter->smps_force_mode_status;
	adapter->smps_force_mode_status = 0;
	if (0 != ret) {
		hddLog(LOGE, FL("SMPS force mode status: %d "),
		       ret);
		return ret;
	}

chain_mask:
	hddLog(LOG1, FL("Update chain mask to 1x1"));
	ret = wlan_hdd_update_txrx_chain_mask(hdd_ctx, 1);
	if (0 != ret) {
		hddLog(LOGE, FL("Failed to switch to 1x1 mode"));
		return ret;
	}

	/* Update SME SM power save config */
	hal_status = sme_update_mimo_power_save(hdd_ctx->hHal,
					true, HDD_SMPS_MODE_STATIC);
	if (eHAL_STATUS_SUCCESS != hal_status) {
		hddLog(LOG1,
		       FL("Failed to update SMPS config to static: %d"),
		       hal_status);
		return -EFAULT;
	}

	hddLog(LOG1, FL("Successfully switched to 1x1 mode"));
	return 0;
}

/**
 * switch_to_2x2_connected_sta_state() - Dynamic switch to 2x2
 * antenna mode in standalone station
 * @hdd_ctx: Pointer to hdd contex
 * @adapter: Pointer to hdd adapter
 *
 * Return: 0 if success else non zero
 */
static int switch_to_2x2_connected_sta_state(hdd_context_t *hdd_ctx,
					     hdd_adapter_t *adapter)
{
	int ret;
	eHalStatus hal_status;
	bool send_smps;

	/*  Check TDLS status and update antenna mode */
	ret = wlan_hdd_tdls_antenna_switch(hdd_ctx, adapter,
					   HDD_ANTENNA_MODE_2X2);
	if (0 != ret)
		return ret;

	hddLog(LOG1, FL("Update chain mask to 2x2"));
	ret = wlan_hdd_update_txrx_chain_mask(hdd_ctx, 3);
	if (0 != ret) {
		hddLog(LOGE, FL("Failed to switch to 2x2 mode"));
		return ret;
	}

	/* If intersection of sta and AP NSS is 1x1 then
	 * skip SMPS indication to AP.
	 */
	send_smps = sme_is_sta_smps_allowed(hdd_ctx->hHal,
					adapter->sessionId);
	if (!send_smps) {
		hddLog(LOGE, FL("Need not indicate SMPS to AP"));
		goto exit;
	}

	hddLog(LOG1, FL("Send SMPS force mode command "));

	/* No need to block on SMPS force mode event when
	 * the mode switch is 2x2 since the chain mask
	 * has already been updated to 2x2
	 */
	ret = process_wma_set_command((int)adapter->sessionId,
				WMI_STA_SMPS_FORCE_MODE_CMDID,
				WMI_SMPS_FORCED_MODE_DISABLED,
				VDEV_CMD);
	if (0 != ret) {
		hddLog(LOGE,
		       FL("Failed to send SMPS force mode to disabled"));
		return ret;
	}

exit:
	/* Update SME SM power save config */
	hal_status = sme_update_mimo_power_save(hdd_ctx->hHal,
				false, HDD_SMPS_MODE_DISABLED);
	if (eHAL_STATUS_SUCCESS != hal_status) {
		hddLog(LOG1,
		       FL("Failed to update SMPS config to disable: %d"),
		       hal_status);
		return -EFAULT;
	}

	hddLog(LOG1, FL("Successfully switched to 2x2 mode"));
	return 0;
}

/**
 * drv_cmd_set_antenna_mode() - SET ANTENNA MODE driver command
 * handler
 * @hdd_ctx: Pointer to hdd context
 * @cmd: Pointer to input command
 * @command_len: Command length
 *
 * Return: 0 for success non-zero for failure
 */
static int drv_cmd_set_antenna_mode(hdd_adapter_t *adapter,
				    uint8_t *command,
				    uint8_t cmd_len)
{
	int ret;
	int mode;
	uint8_t *value = command;
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	if ((hdd_ctx->concurrency_mode > 1) ||
	    (hdd_ctx->no_of_active_sessions[WLAN_HDD_INFRA_STATION] > 1)) {
		hddLog(LOGE, FL("Operation invalid in non sta or concurrent mode"));
		ret = -EPERM;
		goto exit;
	}

	ret = hdd_parse_setantennamode_command(value, &mode);
	if (0 != ret) {
		hddLog(LOGE, FL("Invalid SETANTENNA command"));
		goto exit;
	}

	hddLog(LOG1, FL("Request to switch antenna mode to: %d"), mode);

	if (hdd_ctx->current_antenna_mode == mode) {
		hddLog(LOGE, FL("System already in the requested mode"));
		ret = 0;
		goto exit;
	}

	if ((HDD_ANTENNA_MODE_2X2 == mode) &&
	    (!hdd_is_supported_chain_mask_2x2(hdd_ctx))) {
		hddLog(LOGE, FL("System does not support 2x2 mode"));
		ret = -EPERM;
		goto exit;
	}

	if ((HDD_ANTENNA_MODE_1X1 == mode) &&
	    hdd_is_supported_chain_mask_1x1(hdd_ctx)) {
		hddLog(LOGE, FL("System already in 1x1 mode"));
		ret = 0;
		goto exit;
	}

	/* Non connected state */
	if (0 == wlan_hdd_get_active_session_count(hdd_ctx)) {
		hddLog(LOG1,
		       FL("Switch to %d x %d in non connected state"),
		       mode, mode);

		ret = switch_antenna_mode_non_conn_state(
				hdd_ctx, adapter, mode);
		if (0 != ret) {
			hddLog(LOGE,
			       FL("Failed to switch to %d x %d mode"),
			       mode, mode);
			goto exit;
		}

		hdd_ctx->current_antenna_mode = (mode == 1) ?
			HDD_ANTENNA_MODE_1X1 : HDD_ANTENNA_MODE_2X2;

	} else if ((hdd_ctx->concurrency_mode <= 1) &&
		   (hdd_ctx->no_of_active_sessions[
		    WLAN_HDD_INFRA_STATION] <= 1)) {
		hddLog(LOG1,
		       FL("Switch to %d x %d in connected sta state"),
		       mode, mode);

		if (HDD_ANTENNA_MODE_1X1 == mode) {
			ret = switch_to_1x1_connected_sta_state(
				hdd_ctx, adapter);
			if (0 != ret) {
				hddLog(LOGE,
				       FL("Failed to switch to 1x1 mode"));
				goto exit;
			}
			hdd_ctx->current_antenna_mode =
				HDD_ANTENNA_MODE_1X1;

		} else if (HDD_ANTENNA_MODE_2X2 == mode) {
			ret = switch_to_2x2_connected_sta_state(
				hdd_ctx, adapter);
			if (0 != ret) {
				hddLog(LOGE,
				       FL("Failed to switch to 2x2 mode"));
				goto exit;
			}
			hdd_ctx->current_antenna_mode =
				HDD_ANTENNA_MODE_2X2;
		}
		/* Update the user requested nss in the mac context.
		 * This will be used in tdls protocol engine to form tdls
		 * Management frames.
		 */
		sme_update_user_configured_nss(
				hdd_ctx->hHal,
				hdd_ctx->current_antenna_mode);
	}

exit:
	hddLog(LOG1, FL("Set antenna status: %d current mode: %d"),
	       ret, hdd_ctx->current_antenna_mode);
	return ret;
}

/**
 * drv_cmd_get_antenna_mode() - GET ANTENNA MODE driver command
 * handler
 * @adapter: Pointer to hdd adapter
 * @hdd_ctx: Pointer to hdd context
 * @command: Pointer to input command
 * @command_len: length of the command
 * @priv_data: private data coming with the driver command
 *
 * Return: 0 for success non-zero for failure
 */
static inline int drv_cmd_get_antenna_mode(hdd_adapter_t *adapter,
					   hdd_context_t *hdd_ctx,
					   uint8_t *command,
					   uint8_t command_len,
					   hdd_priv_data_t *priv_data)
{
	uint32_t antenna_mode = 0;
	char extra[32];
	uint8_t len = 0;

	antenna_mode = hdd_ctx->current_antenna_mode;
	len = scnprintf(extra, sizeof(extra), "%s %d", command,
			antenna_mode);
	len = VOS_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hddLog(LOGE, FL("Failed to copy data to user buffer"));
		return -EFAULT;
	}

	return 0;
}

static int hdd_driver_command(hdd_adapter_t *pAdapter,
                              hdd_priv_data_t *ppriv_data)
{
   hdd_priv_data_t priv_data;
   tANI_U8 *command = NULL;
   int ret = 0;

   ENTER();

   if (VOS_FTM_MODE == hdd_get_conparam()) {
        hddLog(LOGE, FL("Command not allowed in FTM mode"));
        return -EINVAL;
   }

   /*
    * Note that valid pointers are provided by caller
    */

   /* copy to local struct to avoid numerous changes to legacy code */
   priv_data = *ppriv_data;

   if (priv_data.total_len <= 0  ||
       priv_data.total_len > WLAN_PRIV_DATA_MAX_LEN)
   {
       hddLog(VOS_TRACE_LEVEL_WARN,
              "%s:invalid priv_data.total_len(%d)!!!", __func__,
              priv_data.total_len);
       ret = -EINVAL;
       goto exit;
   }

   /* Allocate +1 for '\0' */
   command = kmalloc(priv_data.total_len + 1, GFP_KERNEL);
   if (!command)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,
              "%s: failed to allocate memory", __func__);
       ret = -ENOMEM;
       goto exit;
   }

   if (copy_from_user(command, priv_data.buf, priv_data.total_len))
   {
       ret = -EFAULT;
       goto exit;
   }

   /* Make sure the command is NUL-terminated */
   command[priv_data.total_len] = '\0';

   /* at one time the following block of code was conditional. braces
    * have been retained to avoid re-indenting the legacy code
    */
   {
       hdd_context_t *pHddCtx = (hdd_context_t*)pAdapter->pHddCtx;

       hddLog(VOS_TRACE_LEVEL_INFO,
              "%s: Received %s cmd from Wi-Fi GUI***", __func__, command);

       if (strncmp(command, "P2P_DEV_ADDR", 12) == 0 )
       {
           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_P2P_DEV_ADDR_IOCTL,
                            pAdapter->sessionId, (unsigned)
                            (*(pHddCtx->p2pDeviceAddress.bytes+2)<<24 |
                             *(pHddCtx->p2pDeviceAddress.bytes+3)<<16 |
                             *(pHddCtx->p2pDeviceAddress.bytes+4)<<8  |
                             *(pHddCtx->p2pDeviceAddress.bytes+5))));
           if (copy_to_user(priv_data.buf, pHddCtx->p2pDeviceAddress.bytes,
                                                           sizeof(tSirMacAddr)))
           {
               hddLog(VOS_TRACE_LEVEL_ERROR,
                     "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
           }
       }
       else if (strncmp(command, "SETBAND", 7) == 0)
       {
           tANI_U8 *ptr = command ;
           int ret = 0 ;

           /* Change band request received */

           /* First 8 bytes will have "SETBAND " and
            * 9 byte will have band setting value */
           hddLog(VOS_TRACE_LEVEL_INFO,
                  "%s: SetBandCommand Info  comm %s UL %d, TL %d",
                  __func__, command, priv_data.used_len, priv_data.total_len);
           /* Change band request received */
           ret = hdd_setBand_helper(pAdapter->dev, ptr);
       }
       else if (strncmp(command, "SETWMMPS", 8) == 0)
       {
           tANI_U8 *ptr = command;
           ret = hdd_wmmps_helper(pAdapter, ptr);
       }
       else if (strncasecmp(command, "COUNTRY", 7) == 0)
       {
           eHalStatus status;
           unsigned long rc;
           char *country_code;

           country_code = command + 8;

           INIT_COMPLETION(pAdapter->change_country_code);
           hdd_checkandupdate_dfssetting(pAdapter, country_code);

           status =
              sme_ChangeCountryCode(pHddCtx->hHal,
                                    (void *)(tSmeChangeCountryCallback)
                                    wlan_hdd_change_country_code_callback,
                                    country_code, pAdapter,
                                    pHddCtx->pvosContext,
                                    eSIR_TRUE, eSIR_TRUE);
           if (status == eHAL_STATUS_SUCCESS)
           {
               rc = wait_for_completion_timeout(
                       &pAdapter->change_country_code,
                       msecs_to_jiffies(WLAN_WAIT_TIME_COUNTRY));
               if (!rc) {
                   hddLog(VOS_TRACE_LEVEL_ERROR,
                          "%s: SME while setting country code timed out",
                          __func__);
               }
           }
           else
           {
               hddLog(VOS_TRACE_LEVEL_ERROR,
                      "%s: SME Change Country code fail, status=%d",
                      __func__, status);
               ret = -EINVAL;
           }

       }
       else if (strncmp(command, "SETSUSPENDMODE", 14) == 0)
       {
       }
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
       else if (strncmp(command, "SETROAMTRIGGER", 14) == 0)
       {
           tANI_U8 *value = command;
           tANI_S8 rssi = 0;
           tANI_U8 lookUpThreshold = CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_DEFAULT;
           eHalStatus status = eHAL_STATUS_SUCCESS;

           /* Move pointer to ahead of SETROAMTRIGGER<delimiter> */
           value = value + 15;

           /* Convert the value from ascii to integer */
           ret = kstrtos8(value, 10, &rssi);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed Input value may be out of range[%d - %d]",
                      __func__,
                      CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MIN,
                      CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MAX);
               ret = -EINVAL;
               goto exit;
           }

           lookUpThreshold = abs(rssi);

           if ((lookUpThreshold < CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MIN) ||
               (lookUpThreshold > CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "Neighbor lookup threshold value %d is out of range"
                      " (Min: %d Max: %d)", lookUpThreshold,
                      CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MIN,
                      CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MAX);
               ret = -EINVAL;
               goto exit;
           }

           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_SETROAMTRIGGER_IOCTL,
                            pAdapter->sessionId, lookUpThreshold));
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set Roam trigger"
                      " (Neighbor lookup threshold) = %d", __func__, lookUpThreshold);

           pHddCtx->cfg_ini->nNeighborLookupRssiThreshold = lookUpThreshold;
           status = sme_setNeighborLookupRssiThreshold(pHddCtx->hHal,
                                                       pAdapter->sessionId,
                                                       lookUpThreshold);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to set roam trigger, try again", __func__);
               ret = -EPERM;
               goto exit;
           }

           /* Set Reassoc threshold to (lookup rssi threshold + 5 dBm) */
           pHddCtx->cfg_ini->nNeighborReassocRssiThreshold =
                                                      lookUpThreshold + 5;
           sme_setNeighborReassocRssiThreshold(pHddCtx->hHal,
                                               pAdapter->sessionId,
                                               lookUpThreshold + 5);
       }
       else if (strncmp(command, "GETROAMTRIGGER", 14) == 0)
       {
           tANI_U8 lookUpThreshold =
                              sme_getNeighborLookupRssiThreshold(pHddCtx->hHal);
           int rssi = (-1) * lookUpThreshold;
           char extra[32];
           tANI_U8 len = 0;
           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_GETROAMTRIGGER_IOCTL,
                            pAdapter->sessionId, lookUpThreshold));
           len = scnprintf(extra, sizeof(extra), "%s %d", command, rssi);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETROAMSCANPERIOD", 17) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 roamScanPeriod = 0;
           tANI_U16 neighborEmptyScanRefreshPeriod = CFG_EMPTY_SCAN_REFRESH_PERIOD_DEFAULT;

           /* input refresh period is in terms of seconds */
           /* Move pointer to ahead of SETROAMSCANPERIOD<delimiter> */
           value = value + 18;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &roamScanPeriod);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed Input value may be out of range[%d - %d]",
                      __func__,
                      (CFG_EMPTY_SCAN_REFRESH_PERIOD_MIN/1000),
                      (CFG_EMPTY_SCAN_REFRESH_PERIOD_MAX/1000));
               ret = -EINVAL;
               goto exit;
           }

           if ((roamScanPeriod < (CFG_EMPTY_SCAN_REFRESH_PERIOD_MIN/1000)) ||
               (roamScanPeriod > (CFG_EMPTY_SCAN_REFRESH_PERIOD_MAX/1000)))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "Roam scan period value %d is out of range"
                      " (Min: %d Max: %d)", roamScanPeriod,
                      (CFG_EMPTY_SCAN_REFRESH_PERIOD_MIN/1000),
                      (CFG_EMPTY_SCAN_REFRESH_PERIOD_MAX/1000));
               ret = -EINVAL;
               goto exit;
           }
           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_SETROAMSCANPERIOD_IOCTL,
                            pAdapter->sessionId, roamScanPeriod));
           neighborEmptyScanRefreshPeriod = roamScanPeriod * 1000;

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set roam scan period"
                      " (Empty Scan refresh period) = %d", __func__, roamScanPeriod);

           pHddCtx->cfg_ini->nEmptyScanRefreshPeriod = neighborEmptyScanRefreshPeriod;
           sme_UpdateEmptyScanRefreshPeriod(pHddCtx->hHal,
                                            pAdapter->sessionId,
                                            neighborEmptyScanRefreshPeriod);
       }
       else if (strncmp(command, "GETROAMSCANPERIOD", 17) == 0)
       {
           tANI_U16 nEmptyScanRefreshPeriod =
                                   sme_getEmptyScanRefreshPeriod(pHddCtx->hHal);
           char extra[32];
           tANI_U8 len = 0;

           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_GETROAMSCANPERIOD_IOCTL,
                            pAdapter->sessionId, nEmptyScanRefreshPeriod));
           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETROAMSCANPERIOD", (nEmptyScanRefreshPeriod/1000));
           /* Returned value is in units of seconds */
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETROAMSCANREFRESHPERIOD", 24) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 roamScanRefreshPeriod = 0;
           tANI_U16 neighborScanRefreshPeriod = CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_DEFAULT;

           /* input refresh period is in terms of seconds */
           /* Move pointer to ahead of SETROAMSCANREFRESHPERIOD<delimiter> */
           value = value + 25;

           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &roamScanRefreshPeriod);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed Input value may be out of range[%d - %d]",
                      __func__,
                      (CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MIN/1000),
                      (CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MAX/1000));
               ret = -EINVAL;
               goto exit;
           }

           if ((roamScanRefreshPeriod < (CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MIN/1000)) ||
               (roamScanRefreshPeriod > (CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MAX/1000)))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "Neighbor scan results refresh period value %d is out of range"
                      " (Min: %d Max: %d)", roamScanRefreshPeriod,
                      (CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MIN/1000),
                      (CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MAX/1000));
               ret = -EINVAL;
               goto exit;
           }
           neighborScanRefreshPeriod = roamScanRefreshPeriod * 1000;

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set roam scan refresh period"
                      " (Scan refresh period) = %d", __func__, roamScanRefreshPeriod);

           pHddCtx->cfg_ini->nNeighborResultsRefreshPeriod = neighborScanRefreshPeriod;
           sme_setNeighborScanRefreshPeriod(pHddCtx->hHal,
                                            pAdapter->sessionId,
                                            neighborScanRefreshPeriod);
       }
       else if (strncmp(command, "GETROAMSCANREFRESHPERIOD", 24) == 0)
       {
           tANI_U16 value = sme_getNeighborScanRefreshPeriod(pHddCtx->hHal);
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETROAMSCANREFRESHPERIOD", (value/1000));
           /* Returned value is in units of seconds */
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
#ifdef FEATURE_WLAN_LFR
       /* SETROAMMODE */
       else if (strncmp(command, "SETROAMMODE", SIZE_OF_SETROAMMODE) == 0)
       {
           tANI_U8 *value = command;
	   tANI_BOOLEAN roamMode = CFG_LFR_FEATURE_ENABLED_DEFAULT;

	   /* Move pointer to ahead of SETROAMMODE<delimiter> */
	   value = value + SIZE_OF_SETROAMMODE + 1;

	   /* Convert the value from ascii to integer */
	   ret = kstrtou8(value, SIZE_OF_SETROAMMODE, &roamMode);
	   if (ret < 0)
	   {
	      /* If the input value is greater than max value of datatype, then also
		  kstrtou8 fails */
	      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
		   "%s: kstrtou8 failed range [%d - %d]", __func__,
		   CFG_LFR_FEATURE_ENABLED_MIN,
		   CFG_LFR_FEATURE_ENABLED_MAX);
              ret = -EINVAL;
	      goto exit;
	   }
           if ((roamMode < CFG_LFR_FEATURE_ENABLED_MIN) ||
	       (roamMode > CFG_LFR_FEATURE_ENABLED_MAX))
           {
              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
			"Roam Mode value %d is out of range"
			" (Min: %d Max: %d)", roamMode,
			CFG_LFR_FEATURE_ENABLED_MIN,
			CFG_LFR_FEATURE_ENABLED_MAX);
	      ret = -EINVAL;
	      goto exit;
	   }

	   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_DEBUG,
		   "%s: Received Command to Set Roam Mode = %d", __func__, roamMode);
           /*
	    * Note that
	    *     SETROAMMODE 0 is to enable LFR while
	    *     SETROAMMODE 1 is to disable LFR, but
	    *     NotifyIsFastRoamIniFeatureEnabled 0/1 is to enable/disable.
	    *     So, we have to invert the value to call sme_UpdateIsFastRoamIniFeatureEnabled.
	    */
	   if (CFG_LFR_FEATURE_ENABLED_MIN == roamMode)
	       roamMode = CFG_LFR_FEATURE_ENABLED_MAX;    /* Roam enable */
	   else
	       roamMode = CFG_LFR_FEATURE_ENABLED_MIN;    /* Roam disable */

	   pHddCtx->cfg_ini->isFastRoamIniFeatureEnabled = roamMode;
	   /* LFR2 is dependent on Fast Roam. So, enable/disable LFR2
	    * variable. if Fast Roam has been changed from disabled to enabled,
	    * then enable LFR2 and send the LFR START command to the firmware.
	    * Otherwise, send the LFR STOP command to the firmware and then
	    * disable LFR2.The sequence is different.
	    */
	   if (roamMode) {
		   pHddCtx->cfg_ini->isRoamOffloadScanEnabled = roamMode;
		   sme_UpdateRoamScanOffloadEnabled((tHalHandle)(pHddCtx->hHal),
				   pHddCtx->cfg_ini->isRoamOffloadScanEnabled);
		   sme_UpdateIsFastRoamIniFeatureEnabled(pHddCtx->hHal,
				   pAdapter->sessionId,
				   roamMode);
	   } else {
		   sme_UpdateIsFastRoamIniFeatureEnabled(pHddCtx->hHal,
				   pAdapter->sessionId,
				   roamMode);
		   pHddCtx->cfg_ini->isRoamOffloadScanEnabled = roamMode;
		   sme_UpdateRoamScanOffloadEnabled((tHalHandle)(pHddCtx->hHal),
				   pHddCtx->cfg_ini->isRoamOffloadScanEnabled);
	   }
       }
       /* GETROAMMODE */
       else if (strncmp(command, "GETROAMMODE", SIZE_OF_GETROAMMODE) == 0)
       {
	   tANI_BOOLEAN roamMode = sme_getIsLfrFeatureEnabled(pHddCtx->hHal);
	   char extra[32];
	   tANI_U8 len = 0;

           /*
            * roamMode value shall be inverted because the semantics is
            * different.
            */
           if (CFG_LFR_FEATURE_ENABLED_MIN == roamMode)
	       roamMode = CFG_LFR_FEATURE_ENABLED_MAX;
           else
	       roamMode = CFG_LFR_FEATURE_ENABLED_MIN;

	   len = scnprintf(extra, sizeof(extra), "%s %d", command, roamMode);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
	   }
       }
#endif
#endif
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
       else if (strncmp(command, "SETROAMDELTA", 12) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 roamRssiDiff = CFG_ROAM_RSSI_DIFF_DEFAULT;

           /* Move pointer to ahead of SETROAMDELTA<delimiter> */
           value = value + 13;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &roamRssiDiff);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_ROAM_RSSI_DIFF_MIN,
                      CFG_ROAM_RSSI_DIFF_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((roamRssiDiff < CFG_ROAM_RSSI_DIFF_MIN) ||
               (roamRssiDiff > CFG_ROAM_RSSI_DIFF_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "Roam rssi diff value %d is out of range"
                      " (Min: %d Max: %d)", roamRssiDiff,
                      CFG_ROAM_RSSI_DIFF_MIN,
                      CFG_ROAM_RSSI_DIFF_MAX);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set roam rssi diff = %d", __func__, roamRssiDiff);

           pHddCtx->cfg_ini->RoamRssiDiff = roamRssiDiff;
           sme_UpdateRoamRssiDiff(pHddCtx->hHal,
                                  pAdapter->sessionId, roamRssiDiff);
       }
       else if (strncmp(command, "GETROAMDELTA", 12) == 0)
       {
           tANI_U8 roamRssiDiff = sme_getRoamRssiDiff(pHddCtx->hHal);
           char extra[32];
           tANI_U8 len = 0;

           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_GETROAMDELTA_IOCTL,
                            pAdapter->sessionId, roamRssiDiff));
           len = scnprintf(extra, sizeof(extra), "%s %d",
                   command, roamRssiDiff);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
#endif
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
       else if (strncmp(command, "GETBAND", 7) == 0)
       {
           int band = -1;
           char extra[32];
           tANI_U8 len = 0;
           hdd_getBand_helper(pHddCtx, &band);

           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_GETBAND_IOCTL,
                            pAdapter->sessionId, band));
           len = scnprintf(extra, sizeof(extra), "%s %d", command, band);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETROAMSCANCHANNELS ", 20) == 0)
       {
           ret = hdd_parse_set_roam_scan_channels(pAdapter, command);
       }
       else if (strncmp(command, "GETROAMSCANCHANNELS", 19) == 0)
       {
           tANI_U8 ChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
           tANI_U8 numChannels = 0;
           tANI_U8 j = 0;
           char extra[128] = {0};
           int len;

           if (eHAL_STATUS_SUCCESS != sme_getRoamScanChannelList(
                                                           pHddCtx->hHal,
                                                           ChannelList,
                                                           &numChannels,
                                                           pAdapter->sessionId))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s: failed to get roam scan channel list", __func__);
               ret = -EFAULT;
               goto exit;
           }
           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_GETROAMSCANCHANNELS_IOCTL,
                            pAdapter->sessionId, numChannels));
           /* output channel list is of the format
           [Number of roam scan channels][Channel1][Channel2]... */
           /* copy the number of channels in the 0th index */
           len = scnprintf(extra, sizeof(extra), "%s %d", command, numChannels);
           for (j = 0; (j < numChannels); j++)
           {
               len += scnprintf(extra + len, sizeof(extra) - len, " %d",
                       ChannelList[j]);
           }

           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "GETCCXMODE", 10) == 0)
       {
           tANI_BOOLEAN eseMode = sme_getIsEseFeatureEnabled(pHddCtx->hHal);
           char extra[32];
           tANI_U8 len = 0;

           /* Check if the features OKC/ESE/11R are supported simultaneously,
              then this operation is not permitted (return FAILURE) */
           if (eseMode &&
               hdd_is_okc_mode_enabled(pHddCtx) &&
               sme_getIsFtFeatureEnabled(pHddCtx->hHal))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                  "%s: OKC/ESE/11R are supported simultaneously"
                  " hence this operation is not permitted!", __func__);
               ret = -EPERM;
               goto exit;
           }

           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETCCXMODE", eseMode);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "GETOKCMODE", 10) == 0)
       {
           tANI_BOOLEAN okcMode = hdd_is_okc_mode_enabled(pHddCtx);
           char extra[32];
           tANI_U8 len = 0;

           /* Check if the features OKC/ESE/11R are supported simultaneously,
              then this operation is not permitted (return FAILURE) */
           if (okcMode &&
               sme_getIsEseFeatureEnabled(pHddCtx->hHal) &&
               sme_getIsFtFeatureEnabled(pHddCtx->hHal))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                  "%s: OKC/ESE/11R are supported simultaneously"
                  " hence this operation is not permitted!", __func__);
               ret = -EPERM;
               goto exit;
           }

           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETOKCMODE", okcMode);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "GETFASTROAM", 11) == 0)
       {
           tANI_BOOLEAN lfrMode = sme_getIsLfrFeatureEnabled(pHddCtx->hHal);
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETFASTROAM", lfrMode);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "GETFASTTRANSITION", 17) == 0)
       {
           tANI_BOOLEAN ft = sme_getIsFtFeatureEnabled(pHddCtx->hHal);
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETFASTTRANSITION", ft);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETROAMSCANCHANNELMINTIME", 25) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 minTime = CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_DEFAULT;

           /* Move pointer to ahead of SETROAMSCANCHANNELMINTIME<delimiter> */
           value = value + 26;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &minTime);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MIN,
                      CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MAX);
               ret = -EINVAL;
               goto exit;
           }
           if ((minTime < CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MIN) ||
               (minTime > CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "scan min channel time value %d is out of range"
                      " (Min: %d Max: %d)", minTime,
                      CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MIN,
                      CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MAX);
               ret = -EINVAL;
               goto exit;
           }

           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_SETROAMSCANCHANNELMINTIME_IOCTL,
                            pAdapter->sessionId, minTime));
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change channel min time = %d", __func__, minTime);

           pHddCtx->cfg_ini->nNeighborScanMinChanTime = minTime;
           sme_setNeighborScanMinChanTime(pHddCtx->hHal,
                                          minTime, pAdapter->sessionId);
       }
       else if (strncmp(command, "SENDACTIONFRAME", 15) == 0)
       {
           ret = hdd_parse_sendactionframe(pAdapter, command);
       }
       else if (strncmp(command, "GETROAMSCANCHANNELMINTIME", 25) == 0)
       {
           tANI_U16 val = sme_getNeighborScanMinChanTime(
                                         pHddCtx->hHal,
                                         pAdapter->sessionId);
           char extra[32];
           tANI_U8 len = 0;

           /* value is interms of msec */
           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETROAMSCANCHANNELMINTIME", val);
           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_GETROAMSCANCHANNELMINTIME_IOCTL,
                            pAdapter->sessionId, val));
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETSCANCHANNELTIME", 18) == 0)
       {
           tANI_U8 *value = command;
           tANI_U16 maxTime = CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_DEFAULT;

           /* Move pointer to ahead of SETSCANCHANNELTIME<delimiter> */
           value = value + 19;
           /* Convert the value from ascii to integer */
           ret = kstrtou16(value, 10, &maxTime);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou16 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou16 failed range [%d - %d]", __func__,
                      CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MIN,
                      CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((maxTime < CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MIN) ||
               (maxTime > CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "lfr mode value %d is out of range"
                      " (Min: %d Max: %d)", maxTime,
                      CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MIN,
                      CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MAX);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change channel max time = %d", __func__, maxTime);

           pHddCtx->cfg_ini->nNeighborScanMaxChanTime = maxTime;
           sme_setNeighborScanMaxChanTime(pHddCtx->hHal,
                                          pAdapter->sessionId, maxTime);
       }
       else if (strncmp(command, "GETSCANCHANNELTIME", 18) == 0)
       {
           tANI_U16 val = sme_getNeighborScanMaxChanTime(pHddCtx->hHal,
                                                         pAdapter->sessionId);
           char extra[32];
           tANI_U8 len = 0;

           /* value is interms of msec */
           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETSCANCHANNELTIME", val);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETSCANHOMETIME", 15) == 0)
       {
           tANI_U8 *value = command;
           tANI_U16 val = CFG_NEIGHBOR_SCAN_TIMER_PERIOD_DEFAULT;

           /* Move pointer to ahead of SETSCANHOMETIME<delimiter> */
           value = value + 16;
           /* Convert the value from ascii to integer */
           ret = kstrtou16(value, 10, &val);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou16 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou16 failed range [%d - %d]", __func__,
                      CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MIN,
                      CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((val < CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MIN) ||
               (val > CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "scan home time value %d is out of range"
                      " (Min: %d Max: %d)", val,
                      CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MIN,
                      CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MAX);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change scan home time = %d", __func__, val);

           pHddCtx->cfg_ini->nNeighborScanPeriod = val;
           sme_setNeighborScanPeriod(pHddCtx->hHal,
                                     pAdapter->sessionId, val);
       }
       else if (strncmp(command, "GETSCANHOMETIME", 15) == 0)
       {
           tANI_U16 val = sme_getNeighborScanPeriod(pHddCtx->hHal,
                                                    pAdapter->sessionId);
           char extra[32];
           tANI_U8 len = 0;

           /* value is interms of msec */
           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETSCANHOMETIME", val);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETROAMINTRABAND", 16) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 val = CFG_ROAM_INTRA_BAND_DEFAULT;

           /* Move pointer to ahead of SETROAMINTRABAND<delimiter> */
           value = value + 17;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &val);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_ROAM_INTRA_BAND_MIN,
                      CFG_ROAM_INTRA_BAND_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((val < CFG_ROAM_INTRA_BAND_MIN) ||
               (val > CFG_ROAM_INTRA_BAND_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "intra band mode value %d is out of range"
                      " (Min: %d Max: %d)", val,
                      CFG_ROAM_INTRA_BAND_MIN,
                      CFG_ROAM_INTRA_BAND_MAX);
               ret = -EINVAL;
               goto exit;
           }
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change intra band = %d", __func__, val);

           pHddCtx->cfg_ini->nRoamIntraBand = val;
           sme_setRoamIntraBand(pHddCtx->hHal, val);
       }
       else if (strncmp(command, "GETROAMINTRABAND", 16) == 0)
       {
           tANI_U16 val = sme_getRoamIntraBand(pHddCtx->hHal);
           char extra[32];
           tANI_U8 len = 0;

           /* value is interms of msec */
           len = scnprintf(extra, sizeof(extra), "%s %d",
                   "GETROAMINTRABAND", val);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETSCANNPROBES", 14) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 nProbes = CFG_ROAM_SCAN_N_PROBES_DEFAULT;

           /* Move pointer to ahead of SETSCANNPROBES<delimiter> */
           value = value + 15;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &nProbes);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_ROAM_SCAN_N_PROBES_MIN,
                      CFG_ROAM_SCAN_N_PROBES_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((nProbes < CFG_ROAM_SCAN_N_PROBES_MIN) ||
               (nProbes > CFG_ROAM_SCAN_N_PROBES_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "NProbes value %d is out of range"
                      " (Min: %d Max: %d)", nProbes,
                      CFG_ROAM_SCAN_N_PROBES_MIN,
                      CFG_ROAM_SCAN_N_PROBES_MAX);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set nProbes = %d", __func__, nProbes);

           pHddCtx->cfg_ini->nProbes = nProbes;
           sme_UpdateRoamScanNProbes(pHddCtx->hHal, pAdapter->sessionId,
                                     nProbes);
       }
       else if (strncmp(command, "GETSCANNPROBES", 14) == 0)
       {
           tANI_U8 val = sme_getRoamScanNProbes(pHddCtx->hHal);
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d", command, val);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETSCANHOMEAWAYTIME", 19) == 0)
       {
           tANI_U8 *value = command;
           tANI_U16 homeAwayTime = CFG_ROAM_SCAN_HOME_AWAY_TIME_DEFAULT;

           /* Move pointer to ahead of SETSCANHOMEAWAYTIME<delimiter> */
           /* input value is in units of msec */
           value = value + 20;
           /* Convert the value from ascii to integer */
           ret = kstrtou16(value, 10, &homeAwayTime);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_ROAM_SCAN_HOME_AWAY_TIME_MIN,
                      CFG_ROAM_SCAN_HOME_AWAY_TIME_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((homeAwayTime < CFG_ROAM_SCAN_HOME_AWAY_TIME_MIN) ||
               (homeAwayTime > CFG_ROAM_SCAN_HOME_AWAY_TIME_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "homeAwayTime value %d is out of range"
                      " (Min: %d Max: %d)", homeAwayTime,
                      CFG_ROAM_SCAN_HOME_AWAY_TIME_MIN,
                      CFG_ROAM_SCAN_HOME_AWAY_TIME_MAX);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set scan away time = %d", __func__, homeAwayTime);
           if (pHddCtx->cfg_ini->nRoamScanHomeAwayTime != homeAwayTime)
           {
               pHddCtx->cfg_ini->nRoamScanHomeAwayTime = homeAwayTime;
               sme_UpdateRoamScanHomeAwayTime(pHddCtx->hHal,
                                              pAdapter->sessionId,
                                              homeAwayTime, eANI_BOOLEAN_TRUE);
           }
       }
       else if (strncmp(command, "GETSCANHOMEAWAYTIME", 19) == 0)
       {
           tANI_U16 val = sme_getRoamScanHomeAwayTime(pHddCtx->hHal);
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d", command, val);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "REASSOC", 7) == 0)
       {
           ret = hdd_parse_reassoc(pAdapter, command);
       }
       else if (strncmp(command, "SETWESMODE", 10) == 0)
       {
           tANI_U8 *value = command;
           tANI_BOOLEAN wesMode = CFG_ENABLE_WES_MODE_NAME_DEFAULT;

           /* Move pointer to ahead of SETWESMODE<delimiter> */
           value = value + 11;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &wesMode);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_ENABLE_WES_MODE_NAME_MIN,
                      CFG_ENABLE_WES_MODE_NAME_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((wesMode < CFG_ENABLE_WES_MODE_NAME_MIN) ||
               (wesMode > CFG_ENABLE_WES_MODE_NAME_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "WES Mode value %d is out of range"
                      " (Min: %d Max: %d)", wesMode,
                      CFG_ENABLE_WES_MODE_NAME_MIN,
                      CFG_ENABLE_WES_MODE_NAME_MAX);
               ret = -EINVAL;
               goto exit;
           }
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set WES Mode rssi diff = %d", __func__, wesMode);

           pHddCtx->cfg_ini->isWESModeEnabled = wesMode;
           sme_UpdateWESMode(pHddCtx->hHal, wesMode, pAdapter->sessionId);
       }
       else if (strncmp(command, "GETWESMODE", 10) == 0)
       {
           tANI_BOOLEAN wesMode = sme_GetWESMode(pHddCtx->hHal);
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d", command, wesMode);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETOPPORTUNISTICRSSIDIFF", 24) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 nOpportunisticThresholdDiff = CFG_OPPORTUNISTIC_SCAN_THRESHOLD_DIFF_DEFAULT;

           /* Move pointer to ahead of SETOPPORTUNISTICRSSIDIFF<delimiter> */
           value = value + 25;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &nOpportunisticThresholdDiff);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed.", __func__);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set Opportunistic Threshold diff = %d",
                      __func__,
                nOpportunisticThresholdDiff);

           sme_SetRoamOpportunisticScanThresholdDiff(pHddCtx->hHal,
                                                   pAdapter->sessionId,
                                                   nOpportunisticThresholdDiff);
       }
       else if (strncmp(priv_data.buf, "GETOPPORTUNISTICRSSIDIFF", 24) == 0)
       {
           tANI_S8 val = sme_GetRoamOpportunisticScanThresholdDiff(
                                                                 pHddCtx->hHal);
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d", command, val);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETROAMRESCANRSSIDIFF", 21) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 nRoamRescanRssiDiff = CFG_ROAM_RESCAN_RSSI_DIFF_DEFAULT;

           /* Move pointer to ahead of SETROAMRESCANRSSIDIFF<delimiter> */
           value = value + 22;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &nRoamRescanRssiDiff);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed.", __func__);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set Roam Rescan RSSI Diff = %d",
                      __func__,
                      nRoamRescanRssiDiff);
           sme_SetRoamRescanRssiDiff(pHddCtx->hHal,
                                     pAdapter->sessionId,
                                     nRoamRescanRssiDiff);
       }
       else if (strncmp(priv_data.buf, "GETROAMRESCANRSSIDIFF", 21) == 0)
       {
           tANI_U8 val = sme_GetRoamRescanRssiDiff(pHddCtx->hHal);
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d", command, val);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
#endif /* WLAN_FEATURE_VOWIFI_11R || FEATURE_WLAN_ESE || FEATURE_WLAN_LFR */
#ifdef FEATURE_WLAN_LFR
       else if (strncmp(command, "SETFASTROAM", 11) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 lfrMode = CFG_LFR_FEATURE_ENABLED_DEFAULT;

           /* Move pointer to ahead of SETFASTROAM<delimiter> */
           value = value + 12;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &lfrMode);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_LFR_FEATURE_ENABLED_MIN,
                      CFG_LFR_FEATURE_ENABLED_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((lfrMode < CFG_LFR_FEATURE_ENABLED_MIN) ||
               (lfrMode > CFG_LFR_FEATURE_ENABLED_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "lfr mode value %d is out of range"
                      " (Min: %d Max: %d)", lfrMode,
                      CFG_LFR_FEATURE_ENABLED_MIN,
                      CFG_LFR_FEATURE_ENABLED_MAX);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change lfr mode = %d", __func__, lfrMode);

           pHddCtx->cfg_ini->isFastRoamIniFeatureEnabled = lfrMode;
           sme_UpdateIsFastRoamIniFeatureEnabled(pHddCtx->hHal,
                                                 pAdapter->sessionId,
                                                 lfrMode);
       }
#endif
#ifdef WLAN_FEATURE_VOWIFI_11R
       else if (strncmp(command, "SETFASTTRANSITION", 17) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 ft = CFG_FAST_TRANSITION_ENABLED_NAME_DEFAULT;

           /* Move pointer to ahead of SETFASTROAM<delimiter> */
           value = value + 18;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &ft);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_FAST_TRANSITION_ENABLED_NAME_MIN,
                      CFG_FAST_TRANSITION_ENABLED_NAME_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((ft < CFG_FAST_TRANSITION_ENABLED_NAME_MIN) ||
               (ft > CFG_FAST_TRANSITION_ENABLED_NAME_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "ft mode value %d is out of range"
                      " (Min: %d Max: %d)", ft,
                      CFG_FAST_TRANSITION_ENABLED_NAME_MIN,
                      CFG_FAST_TRANSITION_ENABLED_NAME_MAX);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change ft mode = %d", __func__, ft);

           pHddCtx->cfg_ini->isFastTransitionEnabled = ft;
           sme_UpdateFastTransitionEnabled(pHddCtx->hHal, ft);
       }

       else if (strncmp(command, "FASTREASSOC", 11) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 channel = 0;
           tSirMacAddr targetApBssid;
           tHalHandle hHal;
           v_U32_t roamId = 0;
           tCsrRoamModifyProfileFields modProfileFields;
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
           tCsrHandoffRequest handoffInfo;
#endif
           hdd_station_ctx_t *pHddStaCtx = NULL;
           pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
           hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);

           /* if not associated, no need to proceed with reassoc */
           if (eConnectionState_Associated != pHddStaCtx->conn_info.connState)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s:Not associated!",__func__);
               ret = -EINVAL;
               goto exit;
           }

           ret = hdd_parse_reassoc_command_v1_data(value, targetApBssid,
                                                   &channel);
           if (ret)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                          "%s: Failed to parse reassoc command data", __func__);
               goto exit;
           }

           /* if the target bssid is same as currently associated AP,
              issue reassoc to same AP */
           if (VOS_TRUE == vos_mem_compare(targetApBssid,
                                           pHddStaCtx->conn_info.bssId, sizeof(tSirMacAddr)))
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s:Reassoc BSSID is same as currently associated AP bssid",__func__);
               sme_GetModifyProfileFields(hHal, pAdapter->sessionId,
                                       &modProfileFields);
               sme_RoamReassoc(hHal, pAdapter->sessionId,
                            NULL, modProfileFields, &roamId, 1);
               ret = 0;
               goto exit;
           }

           /* Check channel number is a valid channel number */
           if(VOS_STATUS_SUCCESS !=
                         wlan_hdd_validate_operation_channel(pAdapter, channel))
           {
               hddLog(LOGE, FL("Invalid Channel [%d]"), channel);

               ret = -EINVAL;
               goto exit;
           }
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
           if (pHddCtx->cfg_ini->isRoamOffloadEnabled) {
               hdd_wma_send_fastreassoc_cmd((int)pAdapter->sessionId, targetApBssid,
                                       (int) channel);
               goto exit;
           }
#endif
           /* Proceed with reassoc */
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
           handoffInfo.channel = channel;
           handoffInfo.src = FASTREASSOC;
           vos_mem_copy(handoffInfo.bssid, targetApBssid, sizeof(tSirMacAddr));
           sme_HandoffRequest(pHddCtx->hHal, pAdapter->sessionId, &handoffInfo);
#endif
       }
#endif
#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
       else if (strncmp(command, "CCXPLMREQ", 9) == 0)
       {
           tANI_U8 *value = command;
           eHalStatus status = eHAL_STATUS_SUCCESS;
           tpSirPlmReq pPlmRequest = NULL;

           pPlmRequest = vos_mem_malloc(sizeof(tSirPlmReq));
           if (NULL == pPlmRequest){
               ret = -EINVAL;
               goto exit;
           }

           status = hdd_parse_plm_cmd(value, pPlmRequest);
           if (eHAL_STATUS_SUCCESS != status){
               vos_mem_free(pPlmRequest);
               pPlmRequest = NULL;
               ret = -EINVAL;
               goto exit;
           }
           pPlmRequest->sessionId = pAdapter->sessionId;

           status = sme_SetPlmRequest(pHddCtx->hHal, pPlmRequest);
           if (eHAL_STATUS_SUCCESS != status)
           {
               vos_mem_free(pPlmRequest);
               pPlmRequest = NULL;
               ret = -EINVAL;
               goto exit;
           }
       }
#endif
#ifdef FEATURE_WLAN_ESE
       else if (strncmp(command, "SETCCXMODE", 10) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 eseMode = CFG_ESE_FEATURE_ENABLED_DEFAULT;

           /* Check if the features OKC/ESE/11R are supported simultaneously,
              then this operation is not permitted (return FAILURE) */
           if (sme_getIsEseFeatureEnabled(pHddCtx->hHal) &&
               hdd_is_okc_mode_enabled(pHddCtx) &&
               sme_getIsFtFeatureEnabled(pHddCtx->hHal))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                  "%s: OKC/ESE/11R are supported simultaneously"
                  " hence this operation is not permitted!", __func__);
               ret = -EPERM;
               goto exit;
           }

           /* Move pointer to ahead of SETCCXMODE<delimiter> */
           value = value + 11;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &eseMode);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_ESE_FEATURE_ENABLED_MIN,
                      CFG_ESE_FEATURE_ENABLED_MAX);
               ret = -EINVAL;
               goto exit;
           }
           if ((eseMode < CFG_ESE_FEATURE_ENABLED_MIN) ||
               (eseMode > CFG_ESE_FEATURE_ENABLED_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "Ese mode value %d is out of range"
                      " (Min: %d Max: %d)", eseMode,
                      CFG_ESE_FEATURE_ENABLED_MIN,
                      CFG_ESE_FEATURE_ENABLED_MAX);
               ret = -EINVAL;
               goto exit;
           }
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change ese mode = %d", __func__, eseMode);

           pHddCtx->cfg_ini->isEseIniFeatureEnabled = eseMode;
           sme_UpdateIsEseFeatureEnabled(pHddCtx->hHal, pAdapter->sessionId,
                                         eseMode);
       }
#endif
       else if (strncmp(command, "SETROAMSCANCONTROL", 18) == 0)
       {
           tANI_U8 *value = command;
           tANI_BOOLEAN roamScanControl = 0;

           /* Move pointer to ahead of SETROAMSCANCONTROL<delimiter> */
           value = value + 19;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &roamScanControl);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed ", __func__);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set roam scan control = %d", __func__, roamScanControl);

           if (0 != roamScanControl)
           {
               ret = 0; /* return success but ignore param value "TRUE" */
               goto exit;
           }

           sme_SetRoamScanControl(pHddCtx->hHal,
                                  pAdapter->sessionId, roamScanControl);
       }
#ifdef FEATURE_WLAN_OKC
       else if (strncmp(command, "SETOKCMODE", 10) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 okcMode = CFG_OKC_FEATURE_ENABLED_DEFAULT;

           /* Check if the features OKC/ESE/11R are supported simultaneously,
              then this operation is not permitted (return FAILURE) */
           if (sme_getIsEseFeatureEnabled(pHddCtx->hHal) &&
               hdd_is_okc_mode_enabled(pHddCtx) &&
               sme_getIsFtFeatureEnabled(pHddCtx->hHal))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                  "%s: OKC/ESE/11R are supported simultaneously"
                  " hence this operation is not permitted!", __func__);
               ret = -EPERM;
               goto exit;
           }

           /* Move pointer to ahead of SETOKCMODE<delimiter> */
           value = value + 11;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &okcMode);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_OKC_FEATURE_ENABLED_MIN,
                      CFG_OKC_FEATURE_ENABLED_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((okcMode < CFG_OKC_FEATURE_ENABLED_MIN) ||
               (okcMode > CFG_OKC_FEATURE_ENABLED_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "Okc mode value %d is out of range"
                      " (Min: %d Max: %d)", okcMode,
                      CFG_OKC_FEATURE_ENABLED_MIN,
                      CFG_OKC_FEATURE_ENABLED_MAX);
               ret = -EINVAL;
               goto exit;
           }

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change okc mode = %d", __func__, okcMode);
           pHddCtx->cfg_ini->isOkcIniFeatureEnabled = okcMode;
       }
#endif  /* FEATURE_WLAN_OKC */
       else if (strncmp(command, "BTCOEXMODE", 10) == 0 )
       {
           char *bcMode;
           int ret;

           bcMode = command + 11;
           if ('1' == *bcMode)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_DEBUG,
                         FL("BTCOEXMODE %d"), *bcMode);
               pHddCtx->btCoexModeSet = TRUE;
               ret = wlan_hdd_scan_abort(pAdapter);
               if (ret < 0) {
                   hddLog(LOGE,
                          FL("Failed to abort existing scan status:%d"), ret);
               }
           }
           else if ('2' == *bcMode)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_DEBUG,
                         FL("BTCOEXMODE %d"), *bcMode);
               pHddCtx->btCoexModeSet = FALSE;
           }
       }
       else if (strncmp(command, "GETROAMSCANCONTROL", 18) == 0)
       {
           tANI_BOOLEAN roamScanControl = sme_GetRoamScanControl(pHddCtx->hHal);
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d",
                   command, roamScanControl);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
#ifdef WLAN_FEATURE_PACKET_FILTERING
       else if (strncmp(command, "ENABLE_PKTFILTER_IPV6", 21) == 0)
       {
           tANI_U8 filterType = 0;
           tANI_U8 *value = command;

           /* Move pointer to ahead of ENABLE_PKTFILTER_IPV6<delimiter> */
           value = value + 22;

           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &filterType);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype,
                * then also kstrtou8 fails
                */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range ", __func__);
               ret = -EINVAL;
               goto exit;
           }

           if (filterType != 0 && filterType != 1)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: Accepted Values are 0 and 1 ", __func__);
               ret = -EINVAL;
               goto exit;
           }
           wlan_hdd_setIPv6Filter(WLAN_HDD_GET_CTX(pAdapter), filterType,
                   pAdapter->sessionId);
       }
#endif
       else if (strncmp(command, "BTCOEXMODE", 10) == 0 )
       {
           char *bcMode;
           bcMode = command + 11;
           if ('1' == *bcMode)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_DEBUG,
                         FL("BTCOEXMODE %d"), *bcMode);
               pHddCtx->btCoexModeSet = TRUE;
           }
           else if ('2' == *bcMode)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_DEBUG,
                         FL("BTCOEXMODE %d"), *bcMode);
               pHddCtx->btCoexModeSet = FALSE;
           }
       }
       else if (strncmp(command, "SCAN-ACTIVE", 11) == 0)
       {
          hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
          VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                           FL("making default scan to ACTIVE"));
          pHddCtx->ioctl_scan_mode = eSIR_ACTIVE_SCAN;
       }
       else if (strncmp(command, "SCAN-PASSIVE", 12) == 0)
       {
          hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
          VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                           FL("making default scan to PASSIVE"));
          pHddCtx->ioctl_scan_mode = eSIR_PASSIVE_SCAN;
       }
       else if (strncmp(command, "GETDWELLTIME", 12) == 0)
       {
           hdd_config_t *pCfg = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini;
           char extra[32];
           tANI_U8 len = 0;

           memset(extra, 0, sizeof(extra));
           ret = hdd_get_dwell_time(pCfg, command, extra, sizeof(extra), &len);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (ret != 0 || copy_to_user(priv_data.buf, &extra, len))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
           ret = len;
       }
       else if (strncmp(command, "SETDWELLTIME", 12) == 0)
       {
           ret = hdd_set_dwell_time(pAdapter, command);
       }
       else if ( strncasecmp(command, "MIRACAST", 8) == 0 )
       {
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "%s: Received MIRACAST command", __func__);

           ret = hdd_set_miracast_mode(pAdapter, command);
       }
       else if ((strncasecmp(command, "SETIBSSBEACONOUIDATA", 20) == 0) &&
                (WLAN_HDD_IBSS == pAdapter->device_mode))
       {
           int i = 0;
           uint8_t *ibss_ie;
           int32_t command_len;
           int32_t oui_length = 0;
           uint8_t *value = command;
           uint32_t ibss_ie_length;
           tSirModifyIE  ibssModifyIE;
           tCsrRoamProfile   *pRoamProfile;
           hdd_wext_state_t *pWextState =
                WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

           int status;

           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                     "%s: received command %s", __func__, ((char *) value));

           /* validate argument of command */
           if (strlen(value) <= 21) {
               hddLog(LOGE,
                   FL("No arguements in command length %zu"), strlen(value));
               ret = -EFAULT;
               goto exit;
           }

           /* moving to arguments of commands */
           value = value + 21;
           command_len = strlen(value);

           /* oui_data can't be less than 3 bytes */
           if (command_len <= (2 * WLAN_HDD_IBSS_MIN_OUI_DATA_LENGTH)) {
               hddLog(LOGE,
                     FL("Invalid SETIBSSBEACONOUIDATA command length %d"),
                     command_len);
               ret = -EFAULT;
               goto exit;
           }

           ibss_ie = vos_mem_malloc(command_len);
           if (!ibss_ie) {
               hddLog(LOGE,
                     FL("Could not allocate memory for command length %d"),
                     command_len);
               ret = -ENOMEM;
               goto exit;
           }
           vos_mem_zero(ibss_ie, command_len);

           ibss_ie_length = hdd_parse_set_ibss_oui_data_command(value, ibss_ie,
                                                      &oui_length, command_len);
           if (ibss_ie_length < (2 * WLAN_HDD_IBSS_MIN_OUI_DATA_LENGTH)) {
               hddLog(LOGE, FL("Could not parse command %s return length %d"),
                     value, ibss_ie_length);
               ret = -EFAULT;
               vos_mem_free(ibss_ie);
               goto exit;
           }

           pRoamProfile = &pWextState->roamProfile;

           vos_mem_copy(ibssModifyIE.bssid,
                        pRoamProfile->BSSIDs.bssid,
                        VOS_MAC_ADDR_SIZE);

           ibssModifyIE.smeSessionId = pAdapter->sessionId;
           ibssModifyIE.notify = TRUE;
           ibssModifyIE.ieID = IE_EID_VENDOR;
           ibssModifyIE.ieIDLen = ibss_ie_length;
           ibssModifyIE.ieBufferlength = ibss_ie_length;
           ibssModifyIE.pIEBuffer = ibss_ie;
           ibssModifyIE.oui_length = oui_length;

           hddLog(LOGW, FL("ibss_ie length %d oui_length %d ibss_ie:"),
                ibss_ie_length, oui_length);
           while (i < ibssModifyIE.ieBufferlength) {
              hddLog(LOGW, FL("0x%x"), ibss_ie[i++]);
           }

           /* Probe Bcn modification */
           sme_ModifyAddIE(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                        &ibssModifyIE,
                                        eUPDATE_IE_PROBE_BCN);

           /* Populating probe resp frame */
           sme_ModifyAddIE(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                       &ibssModifyIE,
                                       eUPDATE_IE_PROBE_RESP);
           /* free ibss_ie */
           vos_mem_free(ibss_ie);

           status = sme_SendCesiumEnableInd( (tHalHandle)(pHddCtx->hHal),
                         pAdapter->sessionId );
           if (VOS_STATUS_SUCCESS != status) {
                hddLog(LOGE, FL("cesium enable indication failed %d"),
                    status);
                ret = -EINVAL;
                goto exit;
           }

       }
       else if (strncasecmp(command, "SETRMCENABLE", 12) == 0)
       {
          tANI_U8 *value = command;
          tANI_U8 ucRmcEnable = 0;
          int  status;

          if ((WLAN_HDD_IBSS != pAdapter->device_mode) &&
              (WLAN_HDD_SOFTAP != pAdapter->device_mode))
          {
             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Received SETRMCENABLE command in invalid mode %d"
                "SETRMCENABLE command is only allowed in IBSS or SOFTAP mode",
                pAdapter->device_mode);
             ret = -EINVAL;
             goto exit;
          }

          status = hdd_parse_setrmcenable_command(value, &ucRmcEnable);
          if (status)
          {
             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Invalid SETRMCENABLE command ");
             ret = -EINVAL;
             goto exit;
          }

          VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
               "%s: ucRmcEnable %d ", __func__, ucRmcEnable);

          if (TRUE == ucRmcEnable) {
              status = sme_enable_rmc((tHalHandle)(pHddCtx->hHal),
                         pAdapter->sessionId);
          }
          else if(FALSE == ucRmcEnable) {
              status = sme_disable_rmc((tHalHandle)(pHddCtx->hHal),
                         pAdapter->sessionId);
          }
          else
          {
             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Invalid SETRMCENABLE command %d", ucRmcEnable);
             ret = -EINVAL;
             goto exit;
          }

          if (VOS_STATUS_SUCCESS != status)
          {
              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: SETRMC %d failed status %d", __func__, ucRmcEnable,
                 status);
              ret = -EINVAL;
              goto exit;
          }
       }
       else if (strncasecmp(command, "SETRMCACTIONPERIOD", 18) == 0)
       {
          tANI_U8 *value = command;
          tANI_U32 uActionPeriod = 0;
          int  status;

          if ((WLAN_HDD_IBSS != pAdapter->device_mode) &&
              (WLAN_HDD_SOFTAP != pAdapter->device_mode))
          {
             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Received SETRMC command in invalid mode %d"
                "SETRMC command is only allowed in IBSS or SOFTAP mode",
                pAdapter->device_mode);
             ret = -EINVAL;
             goto exit;
          }

          status = hdd_parse_setrmcactionperiod_command(value, &uActionPeriod);
          if (status)
          {
             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Invalid SETRMCACTIONPERIOD command ");
             ret = -EINVAL;
             goto exit;
          }

          VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
               "%s: uActionPeriod %d ", __func__, uActionPeriod);

          if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_RMC_ACTION_PERIOD_FREQUENCY,
                 uActionPeriod, NULL, eANI_BOOLEAN_FALSE))
          {
              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: Could not set SETRMCACTIONPERIOD %d", __func__, uActionPeriod);
              ret = -EINVAL;
              goto exit;
          }

          status = sme_SendRmcActionPeriod( (tHalHandle)(pHddCtx->hHal),
                         pAdapter->sessionId );
          if (VOS_STATUS_SUCCESS != status)
          {
             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Could not send cesium enable indication %d", status);
             ret = -EINVAL;
             goto exit;
          }

      }
      else if (strncasecmp(command, "GETIBSSPEERINFOALL", 18) == 0)
      {
         /* Peer Info All Command */
         int status = eHAL_STATUS_SUCCESS;
         hdd_station_ctx_t *pHddStaCtx = NULL;
         char *extra = NULL;
         int idx = 0, length = 0;
         uint8_t mac_addr[VOS_MAC_ADDR_SIZE];
         v_U32_t numOfBytestoPrint = 0;

         if (WLAN_HDD_IBSS == pAdapter->device_mode)
         {
            pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
         }
         else
         {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: pAdapter is not valid for this device mode",
                      __func__);
            ret = -EINVAL;
            goto exit;
         }

         VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                 "%s: Received GETIBSSPEERINFOALL Command", __func__);

         /* Handle the command */
         status = hdd_cfg80211_get_ibss_peer_info_all(pAdapter);
         if (VOS_STATUS_SUCCESS == status)
         {
            /* The variable extra needed to be allocated on the heap since
             * amount of memory required to copy the data for 32 devices
             * exceeds the size of 1024 bytes of default stack size. On
             * 64 bit devices, the default max stack size of 2048 bytes
             */
            extra = kmalloc(WLAN_MAX_BUF_SIZE, GFP_KERNEL);

            if (NULL == extra)
            {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                         "%s:kmalloc failed", __func__);
               ret = -EINVAL;
               goto exit;
            }

            /* Copy number of stations */
            length = scnprintf( extra, WLAN_MAX_BUF_SIZE, "%d ",
                             pHddStaCtx->ibss_peer_info.numPeers);
            numOfBytestoPrint = length;
            for (idx = 0; idx < pHddStaCtx->ibss_peer_info.numPeers; idx++) {
               int8_t rssi;
               uint32_t tx_rate;

               vos_mem_copy(mac_addr,
                  pHddStaCtx->ibss_peer_info.peerInfoParams[idx].mac_addr,
                  sizeof(mac_addr));

                  tx_rate =
                     pHddStaCtx->ibss_peer_info.peerInfoParams[idx].txRate;
                  rssi = pHddStaCtx->ibss_peer_info.peerInfoParams[idx].rssi;

                  length += scnprintf((extra + length),
                            WLAN_MAX_BUF_SIZE - length,
                            "%02x:%02x:%02x:%02x:%02x:%02x %d %d ",
                            mac_addr[0], mac_addr[1], mac_addr[2],
                            mac_addr[3], mac_addr[4], mac_addr[5],
                            tx_rate, rssi);
                 /*
                  * VOS_TRACE() macro has limitation of 512 bytes for the print
                  * buffer. Hence printing the data in two chunks. The first
                  * chunk will have the data for 16 devices and the second
                  * chunk will have the rest.
                  */
                  if (idx < NUM_OF_STA_DATA_TO_PRINT)
                       numOfBytestoPrint = length;
            }

            /*
             * Copy the data back into buffer, if the data to copy is
             * more than 512 bytes than we will split the data and do
             * it in two shots
             */
            if (copy_to_user(priv_data.buf, extra, numOfBytestoPrint))
            {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                          "%s: Copy into user data buffer failed ", __func__);
               ret = -EFAULT;
               goto exit;
            }
            priv_data.buf[numOfBytestoPrint] = '\0';
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_MED,
                      "%s", priv_data.buf);

            if (length > numOfBytestoPrint)
            {
                if (copy_to_user(priv_data.buf + numOfBytestoPrint,
                                 extra + numOfBytestoPrint,
                                 length - numOfBytestoPrint + 1))
                {
                    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                              "%s: Copy into user data buffer failed ", __func__);
                    ret = -EFAULT;
                    goto exit;
                }
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_MED,
                          "%s", &priv_data.buf[numOfBytestoPrint]);
            }

            /* Free temporary buffer */
            kfree(extra);
         }

         else
         {
            /* Command failed, log error */
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: GETIBSSPEERINFOALL command failed with status code %d",
                      __func__, status);
            ret = -EINVAL;
            goto exit;
         }
         ret = 0;
      }
      else if(strncasecmp(command, "GETIBSSPEERINFO", 15) == 0)
      {
         /* Peer Info <Peer Addr> command */
         tANI_U8 *value = command;
         VOS_STATUS status;
         hdd_station_ctx_t *pHddStaCtx = NULL;
         char extra[128] = { 0 };
         v_U32_t length = 0;
         v_U8_t staIdx = 0;
         v_MACADDR_t peerMacAddr;

         if (WLAN_HDD_IBSS == pAdapter->device_mode)
         {
            pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
         }
         else
         {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: pAdapter is not valid for this device mode",
                      __func__);
            ret = -EINVAL;
            goto exit;
         }

         /* if there are no peers, no need to continue with the command */
         VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                 "%s: Received GETIBSSPEERINFO Command", __func__);

         if (eConnectionState_IbssConnected != pHddStaCtx->conn_info.connState)
         {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s:No IBSS Peers coalesced", __func__);
            ret = -EINVAL;
            goto exit;
         }

         /* Parse the incoming command buffer */
         status = hdd_parse_get_ibss_peer_info(value, &peerMacAddr);
         if (VOS_STATUS_SUCCESS != status)
         {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: Invalid GETIBSSPEERINFO command", __func__);
            ret = -EINVAL;
            goto exit;
         }

         /* Get station index for the peer mac address and sanitize it */
         hdd_Ibss_GetStaId(pHddStaCtx, &peerMacAddr, &staIdx);

         if (staIdx > HDD_MAX_NUM_IBSS_STA)
         {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: Invalid StaIdx %d returned", __func__, staIdx);
            ret = -EINVAL;
            goto exit;
         }

         /* Handle the command */
         status = hdd_cfg80211_get_ibss_peer_info(pAdapter, staIdx);
         if (VOS_STATUS_SUCCESS == status)
         {
           v_U32_t txRate = pHddStaCtx->ibss_peer_info.peerInfoParams[0].txRate;

           length = scnprintf( extra, sizeof(extra), "%d %d", (int)txRate,
                      (int)pHddStaCtx->ibss_peer_info.peerInfoParams[0].rssi);

            /* Copy the data back into buffer */
            if (copy_to_user(priv_data.buf, &extra, length+ 1))
            {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: copy data to user buffer failed GETIBSSPEERINFO command",
                  __func__);
               ret = -EFAULT;
               goto exit;
            }
         }
         else
         {
            /* Command failed, log error */
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: GETIBSSPEERINFO command failed with status code %d",
                      __func__, status);
            ret = -EINVAL;
            goto exit;
         }

         /* Success ! */
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_MED,
                   "%s", priv_data.buf);
         ret = 0;
       }
       else if (strncmp(command, "SETRMCTXRATE", 12) == 0)
       {
          tANI_U8 *value = command;
          tANI_U32 uRate = 0;
          tTxrateinfoflags txFlags = 0;
          tSirRateUpdateInd rateUpdateParams = {0};
          int  status;
          hdd_config_t *pConfig = pHddCtx->cfg_ini;

          if ((WLAN_HDD_IBSS != pAdapter->device_mode) &&
              (WLAN_HDD_SOFTAP != pAdapter->device_mode))
          {
             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Received SETRMCTXRATE command in invalid mode %d"
                "SETRMC command is only allowed in IBSS or SOFTAP mode",
                pAdapter->device_mode);
             ret = -EINVAL;
             goto exit;
          }

          status = hdd_parse_setrmcrate_command(value, &uRate, &txFlags);
          if (status)
          {
             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Invalid SETRMCTXRATE command ");
             ret = -EINVAL;
             goto exit;
          }

          VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
               "%s: uRate %d ", __func__, uRate);

          /* -1 implies ignore this param */
          rateUpdateParams.ucastDataRate = -1;

          /*
           * Fill the user specified RMC rate param
           * and the derived tx flags.
           */
          rateUpdateParams.nss = (pConfig->enable2x2 == 0) ? 0 : 1;
          rateUpdateParams.reliableMcastDataRate = uRate;
          rateUpdateParams.reliableMcastDataRateTxFlag = txFlags;
          rateUpdateParams.dev_mode = pAdapter->device_mode;
          rateUpdateParams.bcastDataRate = -1;
          memcpy(rateUpdateParams.bssid, pAdapter->macAddressCurrent.bytes,
             sizeof(rateUpdateParams.bssid));
          status = sme_SendRateUpdateInd((tHalHandle)(pHddCtx->hHal),
                      &rateUpdateParams);
       }
       else if (strncasecmp(command, "SETIBSSTXFAILEVENT", 18) == 0)
       {
           char *value;
           tANI_U8 tx_fail_count = 0;
           tANI_U16 pid = 0;

           value = command;

           ret = hdd_ParseIBSSTXFailEventParams(value, &tx_fail_count, &pid);

           if (0 != ret)
           {
              hddLog(VOS_TRACE_LEVEL_INFO,
                     "%s: Failed to parse SETIBSSTXFAILEVENT arguments",
                     __func__);
              goto exit;
           }

           hddLog(VOS_TRACE_LEVEL_INFO, "%s: tx_fail_cnt=%hhu, pid=%hu",
                   __func__, tx_fail_count, pid);

           if (0 == tx_fail_count)
           {
               // Disable TX Fail Indication
               if (eHAL_STATUS_SUCCESS  ==
                   sme_TXFailMonitorStartStopInd(pHddCtx->hHal,
                                                 tx_fail_count,
                                                NULL))
               {
                   cesium_pid = 0;
               }
               else
               {
                   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             "%s: failed to disable TX Fail Event ", __func__);
                  ret = -EINVAL;
               }
           }
           else
           {
               if (eHAL_STATUS_SUCCESS  ==
                   sme_TXFailMonitorStartStopInd(pHddCtx->hHal,
                                                 tx_fail_count,
                                           (void*)hdd_tx_fail_ind_callback))
               {
                   cesium_pid = pid;
                   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                             "%s: Registered Cesium pid %u", __func__,
                             cesium_pid);
               }
               else
               {
                   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             "%s: Failed to enable TX Fail Monitoring", __func__);
                   ret = -EINVAL;
               }
           }
       }
#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
       else if (strncmp(command, "SETCCXROAMSCANCHANNELS", 22) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 ChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
           tANI_U8 numChannels = 0;
           eHalStatus status;
           ret = hdd_parse_channellist(value, ChannelList, &numChannels);
           if (ret)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to parse channel list information", __func__);
               goto exit;
           }
           if (numChannels > WNI_CFG_VALID_CHANNEL_LIST_LEN)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD,
                  VOS_TRACE_LEVEL_ERROR,
                  "%s: number of channels (%d) supported exceeded max (%d)",
                  __func__,
                  numChannels,
                  WNI_CFG_VALID_CHANNEL_LIST_LEN);
               ret = -EINVAL;
               goto exit;
           }
           status = sme_SetEseRoamScanChannelList(pHddCtx->hHal,
                                                  pAdapter->sessionId,
                                                  ChannelList,
                                                  numChannels);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to update channel list information", __func__);
               ret = -EINVAL;
               goto exit;
           }
       }
       else if (strncmp(command, "GETTSMSTATS", 11) == 0)
       {
           tANI_U8            *value = command;
           char                extra[128] = {0};
           int                 len = 0;
           tANI_U8             tid = 0;
           hdd_station_ctx_t  *pHddStaCtx = NULL;
           tAniTrafStrmMetrics tsmMetrics;
           pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
           /* if not associated, return error */
           if (eConnectionState_Associated != pHddStaCtx->conn_info.connState)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD,
                         VOS_TRACE_LEVEL_ERROR,
                         "%s:Not associated!",
                         __func__);
               ret = -EINVAL;
               goto exit;
           }
           /* Move pointer to ahead of GETTSMSTATS<delimiter> */
           value = value + 12;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &tid);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype,
                * then also
                * kstrtou8 fails
                */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      TID_MIN_VALUE,
                      TID_MAX_VALUE);
               ret = -EINVAL;
               goto exit;
           }
           if ((tid < TID_MIN_VALUE) || (tid > TID_MAX_VALUE))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "tid value %d is out of range"
                      " (Min: %d Max: %d)", tid,
                      TID_MIN_VALUE,
                      TID_MAX_VALUE);
               ret = -EINVAL;
               goto exit;
           }
           VOS_TRACE( VOS_MODULE_ID_HDD,
                      VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to get tsm stats tid = %d",
                      __func__,
                      tid);
           if (VOS_STATUS_SUCCESS != hdd_get_tsm_stats(pAdapter, tid, &tsmMetrics))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to get tsm stats", __func__);
               ret = -EFAULT;
               goto exit;
           }
           VOS_TRACE( VOS_MODULE_ID_HDD,
                      VOS_TRACE_LEVEL_INFO,
                      "UplinkPktQueueDly(%d)"
                      "UplinkPktQueueDlyHist[0](%d)"
                      "UplinkPktQueueDlyHist[1](%d)"
                      "UplinkPktQueueDlyHist[2](%d)"
                      "UplinkPktQueueDlyHist[3](%d)"
                      "UplinkPktTxDly(%u)"
                      "UplinkPktLoss(%d)"
                      "UplinkPktCount(%d)"
                      "RoamingCount(%d)"
                      "RoamingDly(%d)",
                      tsmMetrics.UplinkPktQueueDly,
                      tsmMetrics.UplinkPktQueueDlyHist[0],
                      tsmMetrics.UplinkPktQueueDlyHist[1],
                      tsmMetrics.UplinkPktQueueDlyHist[2],
                      tsmMetrics.UplinkPktQueueDlyHist[3],
                      tsmMetrics.UplinkPktTxDly,
                      tsmMetrics.UplinkPktLoss,
                      tsmMetrics.UplinkPktCount,
                      tsmMetrics.RoamingCount,
                      tsmMetrics.RoamingDly);
           /* Output TSM stats is of the format
            * GETTSMSTATS [PktQueueDly]
            * [PktQueueDlyHist[0]]:[PktQueueDlyHist[1]] ...[RoamingDly]
            * eg., GETTSMSTATS 10 1:0:0:161 20 1 17 8 39800 */
           len = scnprintf(extra,
                           sizeof(extra),
                           "%s %d %d:%d:%d:%d %u %d %d %d %d",
                           command,
                           tsmMetrics.UplinkPktQueueDly,
                           tsmMetrics.UplinkPktQueueDlyHist[0],
                           tsmMetrics.UplinkPktQueueDlyHist[1],
                           tsmMetrics.UplinkPktQueueDlyHist[2],
                           tsmMetrics.UplinkPktQueueDlyHist[3],
                           tsmMetrics.UplinkPktTxDly,
                           tsmMetrics.UplinkPktLoss,
                           tsmMetrics.UplinkPktCount,
                           tsmMetrics.RoamingCount,
                           tsmMetrics.RoamingDly);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETCCKMIE", 9) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 *cckmIe = NULL;
           tANI_U8 cckmIeLen = 0;
           eHalStatus status = eHAL_STATUS_SUCCESS;
           status = hdd_parse_get_cckm_ie(value, &cckmIe, &cckmIeLen);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to parse cckm ie data", __func__);
               ret = -EINVAL;
               goto exit;
           }
           if (cckmIeLen > DOT11F_IE_RSN_MAX_LEN)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: CCKM Ie input length is more than max[%d]", __func__,
                  DOT11F_IE_RSN_MAX_LEN);
               if (NULL != cckmIe)
               {
                   vos_mem_free(cckmIe);
                   cckmIe = NULL;
               }
               ret = -EINVAL;
               goto exit;
           }
           sme_SetCCKMIe(pHddCtx->hHal, pAdapter->sessionId, cckmIe, cckmIeLen);
           if (NULL != cckmIe)
           {
               vos_mem_free(cckmIe);
               cckmIe = NULL;
           }
       }
       else if (strncmp(command, "CCXBEACONREQ", 12) == 0)
       {
           tANI_U8 *value = command;
           tCsrEseBeaconReq eseBcnReq;
           eHalStatus status = eHAL_STATUS_SUCCESS;

           status = hdd_parse_ese_beacon_req(value, &eseBcnReq);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to parse ese beacon req", __func__);
               ret = -EINVAL;
               goto exit;
           }

           if (!hdd_connIsConnected(WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))) {
               hddLog(VOS_TRACE_LEVEL_INFO, FL("Not associated"));
               hdd_indicateEseBcnReportNoResults (pAdapter,
                                      eseBcnReq.bcnReq[0].measurementToken,
                                      0x02,  //BIT(1) set for measurement done
                                      0);    // no BSS
               goto exit;
           }

           status = sme_SetEseBeaconRequest(pHddCtx->hHal,
                                            pAdapter->sessionId,
                                            &eseBcnReq);

           if (eHAL_STATUS_RESOURCES == status) {
               hddLog(VOS_TRACE_LEVEL_INFO,
                      FL("sme_SetEseBeaconRequest failed (%d),"
                      " a request already in progress"), status);
               ret = -EBUSY;
               goto exit;
           } else if (eHAL_STATUS_SUCCESS != status) {
               VOS_TRACE( VOS_MODULE_ID_HDD,
                          VOS_TRACE_LEVEL_ERROR,
                          "%s: sme_SetEseBeaconRequest failed (%d)",
                         __func__,
                         status);
               ret = -EINVAL;
               goto exit;
           }
       }
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */
       else if (strncmp(command, "SETMCRATE", 9) == 0)
       {
           tANI_U8 *value = command;
           int      targetRate;
           /* Move pointer to ahead of SETMCRATE<delimiter> */
           /* input value is in units of hundred kbps */
           value = value + 10;
           /* Convert the value from ascii to integer, decimal base */
           ret = kstrtouint(value, 10, &targetRate);
           ret = wlan_hdd_set_mc_rate(pAdapter, targetRate);
       }
       else if (strncmp(command, "MAXTXPOWER", 10) == 0)
       {
           int status;
           int txPower;
           VOS_STATUS vosStatus;
           eHalStatus smeStatus;
           tANI_U8 *value = command;
           hdd_adapter_t      *pAdapter;
           tSirMacAddr bssid = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
           tSirMacAddr selfMac = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
           hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;

           status = hdd_parse_setmaxtxpower_command(value, &txPower);
           if (status)
           {
             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Invalid MAXTXPOWER command ");
             ret = -EINVAL;
             goto exit;
           }

           vosStatus = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
           while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == vosStatus )
           {
               pAdapter = pAdapterNode->pAdapter;
               /* Assign correct self MAC address */
               vos_mem_copy(bssid, pAdapter->macAddressCurrent.bytes,
                   VOS_MAC_ADDR_SIZE);
               vos_mem_copy(selfMac, pAdapter->macAddressCurrent.bytes,
                   VOS_MAC_ADDR_SIZE);

               hddLog(VOS_TRACE_LEVEL_INFO, "Device mode %d max tx power %d"
                   " selfMac: " MAC_ADDRESS_STR " bssId: " MAC_ADDRESS_STR " ",
                   pAdapter->device_mode, txPower, MAC_ADDR_ARRAY(selfMac),
                   MAC_ADDR_ARRAY(bssid));
               smeStatus = sme_SetMaxTxPower((tHalHandle)(pHddCtx->hHal), bssid,
                                                              selfMac, txPower);
               if (eHAL_STATUS_SUCCESS != status)
               {
                   hddLog(VOS_TRACE_LEVEL_ERROR, "%s:Set max tx power failed",
                      __func__);
                   ret = -EINVAL;
                   goto exit;
               }
               hddLog(VOS_TRACE_LEVEL_INFO, "%s: Set max tx power success",
                   __func__);
               vosStatus = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pNext);
               pAdapterNode = pNext;
           }
       }
       else if (strncmp(command, "SETDFSSCANMODE", 14) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 dfsScanMode = CFG_ROAMING_DFS_CHANNEL_DEFAULT;

           /* Move pointer to ahead of SETDFSSCANMODE<delimiter> */
           value = value + 15;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &dfsScanMode);
           if (ret < 0)
           {
               /* If the input value is greater than max value of
                * datatype, then also kstrtou8 fails
                */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_ROAMING_DFS_CHANNEL_MIN,
                      CFG_ROAMING_DFS_CHANNEL_MAX);
               ret = -EINVAL;
               goto exit;
           }

           if ((dfsScanMode < CFG_ROAMING_DFS_CHANNEL_MIN) ||
               (dfsScanMode > CFG_ROAMING_DFS_CHANNEL_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "dfsScanMode value %d is out of range"
                      " (Min: %d Max: %d)", dfsScanMode,
                      CFG_ROAMING_DFS_CHANNEL_MIN,
                      CFG_ROAMING_DFS_CHANNEL_MAX);
               ret = -EINVAL;
               goto exit;
           }
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set DFS Scan Mode = %d",
                      __func__, dfsScanMode);

           /* When DFS scanning is disabled, the DFS channels need to be
            * removed from the operation of device.
            */
           ret = wlan_hdd_disable_dfs_chan_scan(pHddCtx, pAdapter,
                        (dfsScanMode == CFG_ROAMING_DFS_CHANNEL_DISABLED));
           if (ret < 0) {
               /* Some conditions prevented it from disabling DFS channels
                */
               hddLog(LOGE,
                   FL("disable/enable DFS channel request was denied"));
               goto exit;
           }

           pHddCtx->cfg_ini->allowDFSChannelRoam = dfsScanMode;
           sme_UpdateDFSScanMode(pHddCtx->hHal, pAdapter->sessionId,
                                 dfsScanMode);
       }
       else if (strncmp(command, "GETDFSSCANMODE", 14) == 0)
       {
           tANI_U8 dfsScanMode = sme_GetDFSScanMode(pHddCtx->hHal);
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d", command, dfsScanMode);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "GETLINKSTATUS", 13) == 0) {
           int value = wlan_hdd_get_link_status(pAdapter);
           char extra[32];
           tANI_U8 len = 0;

           len = scnprintf(extra, sizeof(extra), "%s %d", command, value);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
       else if (strncmp(command, "ENABLEEXTWOW", 12) == 0) {

           tANI_U8 *value = command;
           int set_value;

           /* Move pointer to ahead of ENABLEEXTWOW*/
           value += 12;
           if (!(sscanf(value, "%d", &set_value))) {
                     VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                               FL("No input identified"));
                     ret = -EINVAL;
                     goto exit;
           }
           ret = hdd_enable_ext_wow_parser(pAdapter,
                               pAdapter->sessionId, set_value);

       } else if (strncmp(command, "SETAPP1PARAMS", 13) == 0) {
           tANI_U8 *value = command;

           /* Move pointer to ahead of SETAPP1PARAMS*/
           value += 13;
           ret = hdd_set_app_type1_parser(pAdapter,
                                         value, strlen(value));
           if (ret >= 0)
               pHddCtx->is_extwow_app_type1_param_set = TRUE;

       } else if (strncmp(command, "SETAPP2PARAMS", 13) == 0) {
           tANI_U8 *value = command;

           /* Move pointer to ahead of SETAPP2PARAMS*/
           value += 13;
           ret = hdd_set_app_type2_parser(pAdapter,
                                         value, strlen(value));
           if (ret >= 0)
               pHddCtx->is_extwow_app_type2_param_set = TRUE;
       }
#endif
#ifdef FEATURE_WLAN_TDLS
       else if (strncmp(command, "TDLSSECONDARYCHANNELOFFSET", 26) == 0) {
           tANI_U8 *value = command;
           int set_value;
           /* Move pointer to point the string */
           value += 26;
           if (!(sscanf(value, "%d", &set_value))) {
                     VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                               FL("No input identified"));
               ret = -EINVAL;
               goto exit;
           }
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                     FL("Tdls offchannel offset:%d"),
                     set_value);
           ret = hdd_set_tdls_secoffchanneloffset(pHddCtx, set_value);
       } else if (strncmp(command, "TDLSOFFCHANNELMODE", 18) == 0) {
           tANI_U8 *value = command;
           int set_value;
           /* Move pointer to point the string */
           value += 18;
           if (!(sscanf(value, "%d", &set_value))) {
                     VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                               FL("No input identified"));
               ret = -EINVAL;
               goto exit;
           }
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                     FL("Tdls offchannel mode:%d"),
                     set_value);
           ret = hdd_set_tdls_offchannelmode(pAdapter, set_value);
       } else if (strncmp(command, "TDLSOFFCHANNEL", 14) == 0) {
           tANI_U8 *value = command;
           int set_value;
           /* Move pointer to point the string */
           value += 14;
           ret = sscanf(value, "%d", &set_value);
           if (ret != 1) {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                     "Wrong value is given for hdd_set_tdls_offchannel");
               ret = -EINVAL;
               goto exit;
           }

           if (VOS_IS_DFS_CH(set_value)) {
               hddLog(LOGE,
                     FL("DFS channel %d is passed for hdd_set_tdls_offchannel"),
                     set_value);
               ret = -EINVAL;
               goto exit;
           }
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                FL("Tdls offchannel num: %d"), set_value);
           ret = hdd_set_tdls_offchannel(pHddCtx, set_value);
       } else if (strncmp(command, "TDLSSCAN", 8) == 0) {
           uint8_t *value = command;
           int set_value;
           /* Move pointer to point the string */
           value += 8;
           ret = sscanf(value, "%d", &set_value);
           if (ret != 1) {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                     "Wrong value is given for tdls_scan_type");
               ret = -EINVAL;
               goto exit;
           }
           hddLog(LOG1, FL("Tdls scan type val: %d"),
                  set_value);
           ret = hdd_set_tdls_scan_type(pHddCtx, set_value);
       }
#endif
       else if (strncasecmp(command, "RSSI", 4) == 0) {
           v_S7_t s7Rssi = 0;
           char extra[32];
           tANI_U8 len = 0;

           wlan_hdd_get_rssi(pAdapter, &s7Rssi);

           len = scnprintf(extra, sizeof(extra), "%s %d", command, s7Rssi);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               hddLog(LOGE, FL("Failed to copy data to user buffer"));
               ret = -EFAULT;
               goto exit;
           }
       } else if (strncasecmp(command, "LINKSPEED", 9) == 0) {
           uint32_t link_speed = 0;
           char extra[32];
           uint8_t len = 0;

           ret = wlan_hdd_get_link_speed(pAdapter, &link_speed);
           if (0 != ret)
               goto exit;

           len = scnprintf(extra, sizeof(extra), "%s %d", command, link_speed);
           len = VOS_MIN(priv_data.total_len, len + 1);
           if (copy_to_user(priv_data.buf, &extra, len)) {
               hddLog(LOGE, FL("Failed to copy data to user buffer"));
               ret = -EFAULT;
               goto exit;
           }
       } else if (strncasecmp(command, "SET_FCC_CHANNEL", 15) == 0) {
           /*
            * this command wld be called by user-space when it detects WLAN
            * ON after airplane mode is set. When APM is set, WLAN turns off.
            * But it can be turned back on. Otherwise; when APM is turned back
            * off, WLAN wld turn back on. So at that point the command is
            * expected to come down. 0 means disable, 1 means enable. The
            * constraint is removed when parameter 1 is set or different
            * country code is set
            */

           ret = drv_cmd_set_fcc_channel(pHddCtx, command, 15);

       } else if (strncmp(command, "RXFILTER-REMOVE", 15) == 0) {

           ret = hdd_driver_rxfilter_comand_handler(command, pAdapter, false);

       } else if (strncmp(command, "RXFILTER-ADD", 12) == 0) {

           ret = hdd_driver_rxfilter_comand_handler(command, pAdapter, true);

       } else if (strncasecmp(command, "SETANTENNAMODE", 14) == 0) {
           ret = drv_cmd_set_antenna_mode(pAdapter, command, 14);
           hddLog(LOG1, FL("set antenna mode ret: %d"), ret);
       } else if (strncasecmp(command, "GETANTENNAMODE", 14) == 0) {

           ret = drv_cmd_get_antenna_mode(pAdapter, pHddCtx, command,
                                          14, &priv_data);
           hddLog(LOG1, FL("Get antenna mode ret: %d mode: %s"),
                  ret, priv_data.buf);
       } else {
           MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                            TRACE_CODE_HDD_UNSUPPORTED_IOCTL,
                            pAdapter->sessionId, 0));
           hddLog( VOS_TRACE_LEVEL_WARN, "%s: Unsupported GUI command %s",
                   __func__, command);
       }

   }
exit:
   if (command)
   {
       kfree(command);
   }
   EXIT();
   return ret;
}

#ifdef CONFIG_COMPAT
static int hdd_driver_compat_ioctl(hdd_adapter_t *pAdapter, struct ifreq *ifr)
{
   struct {
      compat_uptr_t buf;
      int used_len;
      int total_len;
   } compat_priv_data;
   hdd_priv_data_t priv_data;
   int ret = 0;

   /*
    * Note that pAdapter and ifr have already been verified by caller,
    * and HDD context has also been validated
    */
   if (copy_from_user(&compat_priv_data, ifr->ifr_data,
                      sizeof(compat_priv_data))) {
       ret = -EFAULT;
       goto exit;
   }
   priv_data.buf = compat_ptr(compat_priv_data.buf);
   priv_data.used_len = compat_priv_data.used_len;
   priv_data.total_len = compat_priv_data.total_len;
   ret = hdd_driver_command(pAdapter, &priv_data);
 exit:
   return ret;
}
#else /* CONFIG_COMPAT */
static int hdd_driver_compat_ioctl(hdd_adapter_t *pAdapter, struct ifreq *ifr)
{
   /* will never be invoked */
   return 0;
}
#endif /* CONFIG_COMPAT */

static int hdd_driver_ioctl(hdd_adapter_t *pAdapter, struct ifreq *ifr)
{
   hdd_priv_data_t priv_data;
   int ret = 0;

   /*
    * Note that pAdapter and ifr have already been verified by caller,
    * and HDD context has also been validated
    */
   if (copy_from_user(&priv_data, ifr->ifr_data, sizeof(priv_data))) {
       ret = -EFAULT;
   } else {
      ret = hdd_driver_command(pAdapter, &priv_data);
   }
   return ret;
}

/**
 * __hdd_ioctl() - HDD ioctl handler
 * @dev: pointer to net_device structure
 * @ifr: pointer to ifreq structure
 * @cmd: ioctl command
 *
 * Return: 0 for success and error number for failure.
 */
static int __hdd_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_context_t *pHddCtx;
   long ret = 0;

   ENTER();

   if (dev != pAdapter->dev) {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                 "%s: HDD adapter/dev inconsistency", __func__);
      ret = -ENODEV;
      goto exit;
   }

   if ((!ifr) || (!ifr->ifr_data))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: invalid data", __func__);
      ret = -EINVAL;
      goto exit;
   }

#if  defined(QCA_WIFI_FTM) && defined(LINUX_QCMBR)
   if (VOS_FTM_MODE == hdd_get_conparam()) {
       if (SIOCIOCTLTX99 == cmd) {
           ret = wlan_hdd_qcmbr_unified_ioctl(pAdapter, ifr);
           goto exit;
       }
   }
#endif

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   ret = wlan_hdd_validate_context(pHddCtx);
   if (ret) {
      ret = -EBUSY;
      goto exit;
   }

   switch (cmd) {
   case (SIOCDEVPRIVATE + 1):
      if (is_compat_task())
         ret = hdd_driver_compat_ioctl(pAdapter, ifr);
      else
         ret = hdd_driver_ioctl(pAdapter, ifr);
      break;
   default:
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: unknown ioctl %d",
             __func__, cmd);
      ret = -EINVAL;
      break;
   }
 exit:
   EXIT();
   return ret;
}

/**
 * hdd_ioctl() - Wrapper function to protect __hdd_ioctl() function from SSR
 * @dev: pointer to net_device structure
 * @ifr: pointer to ifreq structure
 * @cmd: ioctl command
 *
 * Return: 0 for success and error number for failure.
 */
static int hdd_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __hdd_ioctl(dev, ifr, cmd);
	vos_ssr_unprotect(__func__);

	return ret;
}


/*
 * Mac address for multiple virtual interface is found as following
 * i) The mac address of the first interface is just the actual hw mac address.
 * ii) MSM 3 or 4 bits of byte5 of the actual mac address are used to
 *     define the mac address for the remaining interfaces and locally
 *     administered bit is set. INTF_MACADDR_MASK is based on the number of
 *     supported virtual interfaces, right now this is 0x07 (meaning 8 interface).
 *     Byte[3] of second interface will be hw_macaddr[3](bit5..7) + 1,
 *     for third interface it will be hw_macaddr[3](bit5..7) + 2, etc.
 */

void hdd_update_macaddr(hdd_config_t *cfg_ini, v_MACADDR_t hw_macaddr)
{
    int8_t i;
    u_int8_t macaddr_b3, tmp_br3;

    vos_mem_copy(cfg_ini->intfMacAddr[0].bytes, hw_macaddr.bytes,
                 VOS_MAC_ADDR_SIZE);
    for (i = 1; i < VOS_MAX_CONCURRENCY_PERSONA; i++) {
        vos_mem_copy(cfg_ini->intfMacAddr[i].bytes, hw_macaddr.bytes,
                     VOS_MAC_ADDR_SIZE);
        macaddr_b3 = cfg_ini->intfMacAddr[i].bytes[3];
        tmp_br3 = ((macaddr_b3 >> 4 & INTF_MACADDR_MASK) + i) &
                  INTF_MACADDR_MASK;
        macaddr_b3 += tmp_br3;

        /* XOR-ing bit-24 of the mac address. This will give enough
         * mac address range before collision
         */
        macaddr_b3 ^= (1 << 7);

        /* Set locally administered bit */
        cfg_ini->intfMacAddr[i].bytes[0] |= 0x02;
        cfg_ini->intfMacAddr[i].bytes[3] = macaddr_b3;
        hddLog(VOS_TRACE_LEVEL_INFO, "cfg_ini->intfMacAddr[%d]: "
               MAC_ADDRESS_STR, i,
               MAC_ADDR_ARRAY(cfg_ini->intfMacAddr[i].bytes));
    }
}

static void hdd_update_tgt_services(hdd_context_t *hdd_ctx,
                                    struct hdd_tgt_services *cfg)
{
    hdd_config_t *cfg_ini = hdd_ctx->cfg_ini;

    /* Set up UAPSD */
    cfg_ini->apUapsdEnabled &= cfg->uapsd;

#ifdef WLAN_FEATURE_11AC
    /* 11AC mode support */
    if ((cfg_ini->dot11Mode == eHDD_DOT11_MODE_11ac ||
        cfg_ini->dot11Mode == eHDD_DOT11_MODE_11ac_ONLY) &&
                                               !cfg->en_11ac)
        cfg_ini->dot11Mode = eHDD_DOT11_MODE_AUTO;
#endif /* #ifdef WLAN_FEATURE_11AC */

    /* ARP offload: override user setting if invalid  */
    cfg_ini->fhostArpOffload &= cfg->arp_offload;

#ifdef FEATURE_WLAN_SCAN_PNO
    /* PNO offload */
    hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: PNO Capability in f/w = %d",
           __func__,cfg->pno_offload);
    if (cfg->pno_offload)
        cfg_ini->PnoOffload = TRUE;
#endif
    sme_set_lte_coex_supp(hdd_ctx->hHal,
                    cfg->lte_coex_ant_share);
    hdd_ctx->per_band_chainmask_supp = cfg->per_band_chainmask_supp;
    sme_set_per_band_chainmask_supp(hdd_ctx->hHal,
                    cfg->per_band_chainmask_supp);
#ifdef FEATURE_WLAN_TDLS
    cfg_ini->fEnableTDLSSupport &= cfg->en_tdls;
    cfg_ini->fEnableTDLSOffChannel &= cfg->en_tdls_offchan;
    cfg_ini->fEnableTDLSBufferSta &= cfg->en_tdls_uapsd_buf_sta;
    if (cfg_ini->fTDLSUapsdMask && cfg->en_tdls_uapsd_sleep_sta)
    {
        cfg_ini->fEnableTDLSSleepSta = TRUE;
    }
    else
    {
        cfg_ini->fEnableTDLSSleepSta = FALSE;
    }
#endif
    sme_set_bcon_offload_supp(hdd_ctx->hHal, cfg->beacon_offload);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    cfg_ini->isRoamOffloadEnabled &= cfg->en_roam_offload;
#endif

#ifdef SAP_AUTH_OFFLOAD
    cfg_ini->enable_sap_auth_offload &= cfg->sap_auth_offload_service;
#endif
}

/**
 * hdd_update_chain_mask_vdev_nss() - sets the chain mask and vdev nss
 * @hdd_ctx: HDD context
 * @cfg: Pointer to target services.
 *
 * Sets the chain masks for 2G and 5G bands based on target supported
 * values and INI values. And sets the Nss per vdev type based on INI
 * and configured chain mask value.
 *
 * Return: None
 */
static void hdd_update_chain_mask_vdev_nss(hdd_context_t *hdd_ctx,
		struct hdd_tgt_services *cfg)
{
	hdd_config_t *cfg_ini = hdd_ctx->cfg_ini;
	uint8_t chain_mask, ret;
	uint8_t max_supp_nss = 1;

	cfg_ini->enable2x2 = 0;
	chain_mask = cfg->chain_mask_2g & cfg_ini->chain_mask_2g;
	if (!chain_mask)
		chain_mask = cfg->chain_mask_2g;
	hddLog(VOS_TRACE_LEVEL_INFO,
			"%s: set 2G chain mask value %d",
			__func__, chain_mask);
	ret = process_wma_set_command(0, WMI_PDEV_PARAM_RX_CHAIN_MASK_2G,
			chain_mask, PDEV_CMD);
	if (0 != ret) {
		hddLog(VOS_TRACE_LEVEL_ERROR,
			"%s: set WMI_PDEV_PARAM_RX_CHAIN_MASK_2G failed %d",
			__func__, ret);
	}
	ret = process_wma_set_command(0, WMI_PDEV_PARAM_TX_CHAIN_MASK_2G,
			chain_mask, PDEV_CMD);
	if (0 != ret) {
		hddLog(VOS_TRACE_LEVEL_ERROR,
			"%s: WMI_PDEV_PARAM_TX_CHAIN_MASK_2G set failed %d",
			__func__, ret);
	}
	max_supp_nss += ((chain_mask & 0x3) == 0x3);

	if (max_supp_nss == 2)
		cfg_ini->enable2x2 = 1;
	sme_update_vdev_type_nss(hdd_ctx->hHal, max_supp_nss,
			cfg_ini->vdev_type_nss_2g, eCSR_BAND_24);
	hdd_ctx->supp_2g_chain_mask = chain_mask;

	max_supp_nss = 1;
	chain_mask = cfg->chain_mask_5g & cfg_ini->chain_mask_5g;
	if (!chain_mask)
		chain_mask = cfg->chain_mask_5g;
	hddLog(VOS_TRACE_LEVEL_INFO,
			"%s: set 5G chain mask value %d",
			__func__, chain_mask);
	ret = process_wma_set_command(0, WMI_PDEV_PARAM_RX_CHAIN_MASK_5G,
			chain_mask, PDEV_CMD);
	if (0 != ret) {
		hddLog(VOS_TRACE_LEVEL_ERROR,
			"%s: set WMI_PDEV_PARAM_RX_CHAIN_MASK_5G failed %d",
			__func__, ret);
	}
	ret = process_wma_set_command(0, WMI_PDEV_PARAM_TX_CHAIN_MASK_5G,
			chain_mask, PDEV_CMD);
	if (0 != ret) {
		hddLog(VOS_TRACE_LEVEL_ERROR,
			"%s: WMI_PDEV_PARAM_TX_CHAIN_MASK_5G set failed %d",
			__func__, ret);
	}
	max_supp_nss += ((chain_mask & 0x3) == 0x3);

	if (max_supp_nss == 2)
		cfg_ini->enable2x2 = 1;
	sme_update_vdev_type_nss(hdd_ctx->hHal, max_supp_nss,
			cfg_ini->vdev_type_nss_5g, eCSR_BAND_5G);
	hdd_ctx->supp_5g_chain_mask = chain_mask;
	hddLog(LOG1, FL("Supported chain mask 2G: %d 5G: %d"),
	       hdd_ctx->supp_2g_chain_mask,
	       hdd_ctx->supp_5g_chain_mask);
}

static void hdd_update_tgt_ht_cap(hdd_context_t *hdd_ctx,
                                  struct hdd_tgt_ht_cap *cfg)
{
    eHalStatus status;
    tANI_U32 value, val32;
    tANI_U16 val16;
    hdd_config_t *pconfig = hdd_ctx->cfg_ini;
    tSirMacHTCapabilityInfo *phtCapInfo;
    tANI_U8 mcs_set[SIZE_OF_SUPPORTED_MCS_SET];
    uint8_t enable_tx_stbc;

    /* check and update RX STBC */
    if (pconfig->enableRxSTBC && !cfg->ht_rx_stbc)
        pconfig->enableRxSTBC = cfg->ht_rx_stbc;

    /* get the MPDU density */
    status = ccmCfgGetInt(hdd_ctx->hHal, WNI_CFG_MPDU_DENSITY, &value);

    if (status != eHAL_STATUS_SUCCESS) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: could not get MPDU DENSITY",
                  __func__);
        value = 0;
    }

    /*
     * MPDU density:
     * override user's setting if value is larger
     * than the one supported by target
     */
    if (value > cfg->mpdu_density) {
        status = ccmCfgSetInt(hdd_ctx->hHal, WNI_CFG_MPDU_DENSITY,
                              cfg->mpdu_density,
                              NULL, eANI_BOOLEAN_FALSE);

        if (status == eHAL_STATUS_FAILURE)
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "%s: could not set MPDU DENSITY to CCM",
                      __func__);
    }

    /* get the HT capability info*/
    status = ccmCfgGetInt(hdd_ctx->hHal, WNI_CFG_HT_CAP_INFO, &val32);
    if (eHAL_STATUS_SUCCESS != status) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: could not get HT capability info",
                  __func__);
        return;
    }
    val16 = (tANI_U16)val32;
    phtCapInfo = (tSirMacHTCapabilityInfo *)&val16;

    /* Set the LDPC capability */
    phtCapInfo->advCodingCap = cfg->ht_rx_ldpc;


    if (pconfig->ShortGI20MhzEnable && !cfg->ht_sgi_20)
        pconfig->ShortGI20MhzEnable = cfg->ht_sgi_20;

    if (pconfig->ShortGI40MhzEnable && !cfg->ht_sgi_40)
        pconfig->ShortGI40MhzEnable = cfg->ht_sgi_40;

    hdd_ctx->num_rf_chains     = cfg->num_rf_chains;
    hdd_ctx->ht_tx_stbc_supported = cfg->ht_tx_stbc;

    enable_tx_stbc = pconfig->enableTxSTBC;

    if (pconfig->enable2x2 && (hdd_ctx->per_band_chainmask_supp ||
          (!hdd_ctx->per_band_chainmask_supp && (cfg->num_rf_chains == 2))))
    {
        pconfig->enable2x2 = 1;
    }
    else
    {
        pconfig->enable2x2 = 0;
        enable_tx_stbc = 0;

        /* 1x1 */
        /* Update Rx Highest Long GI data Rate */
        if (ccmCfgSetInt(hdd_ctx->hHal,
                    WNI_CFG_VHT_RX_HIGHEST_SUPPORTED_DATA_RATE,
                    VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_1_1, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
        {
            hddLog(LOGE, "Could not pass on "
                    "WNI_CFG_VHT_RX_HIGHEST_SUPPORTED_DATA_RATE to CCM");
        }

        /* Update Tx Highest Long GI data Rate */
        if (ccmCfgSetInt(hdd_ctx->hHal, WNI_CFG_VHT_TX_HIGHEST_SUPPORTED_DATA_RATE,
                    VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_1_1, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
        {
            hddLog(LOGE, "Could not pass on "
                    "HDD_VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_1_1 to CCM");
        }
    }
    if (!(cfg->ht_tx_stbc && pconfig->enable2x2))
    {
        enable_tx_stbc = 0;
    }
    phtCapInfo->txSTBC = enable_tx_stbc;
    val32 = val16;
    status = ccmCfgSetInt(hdd_ctx->hHal, WNI_CFG_HT_CAP_INFO,
                          val32, NULL, eANI_BOOLEAN_FALSE);
    if (status != eHAL_STATUS_SUCCESS)
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                  "%s: could not set HT capability to CCM",
                  __func__);
#define WLAN_HDD_RX_MCS_ALL_NSTREAM_RATES 0xff
    value = SIZE_OF_SUPPORTED_MCS_SET;
    if (ccmCfgGetStr(hdd_ctx->hHal, WNI_CFG_SUPPORTED_MCS_SET, mcs_set,
                     &value) == eHAL_STATUS_SUCCESS) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                  "%s: Read MCS rate set", __func__);

        if (pconfig->enable2x2)
        {
            for (value = 0; value < 2; value++)
                mcs_set[value] = WLAN_HDD_RX_MCS_ALL_NSTREAM_RATES;

            status = ccmCfgSetStr(hdd_ctx->hHal, WNI_CFG_SUPPORTED_MCS_SET,
                                  mcs_set, SIZE_OF_SUPPORTED_MCS_SET, NULL,
                                  eANI_BOOLEAN_FALSE);
            if (status == eHAL_STATUS_FAILURE)
                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                          "%s: could not set MCS SET to CCM", __func__);
        }
    }
#undef WLAN_HDD_RX_MCS_ALL_NSTREAM_RATES
}

#ifdef WLAN_FEATURE_11AC
static void hdd_update_tgt_vht_cap(hdd_context_t *hdd_ctx,
                                   struct hdd_tgt_vht_cap *cfg)
{
    eHalStatus status;
    tANI_U32 value = 0;
    hdd_config_t *pconfig = hdd_ctx->cfg_ini;
    tANI_U32 temp = 0;

    /* Get the current MPDU length */
    status = ccmCfgGetInt(hdd_ctx->hHal, WNI_CFG_VHT_MAX_MPDU_LENGTH, &value);

    if (status != eHAL_STATUS_SUCCESS) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: could not get MPDU LENGTH",
                  __func__);
        value = 0;
    }

    /*
     * VHT max MPDU length:
     * override if user configured value is too high
     * that the target cannot support
     */
    if (value > cfg->vht_max_mpdu) {
        status = ccmCfgSetInt(hdd_ctx->hHal,
                              WNI_CFG_VHT_MAX_MPDU_LENGTH,
                              cfg->vht_max_mpdu, NULL,
                              eANI_BOOLEAN_FALSE);

        if (status == eHAL_STATUS_FAILURE) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "%s: could not set VHT MAX MPDU LENGTH",
                      __func__);
        }
    }

    /* Get the current supported chan width */
    status = ccmCfgGetInt(hdd_ctx->hHal, WNI_CFG_VHT_SUPPORTED_CHAN_WIDTH_SET,
                          &value);

    if (status != eHAL_STATUS_SUCCESS) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: could not get MPDU LENGTH",
                  __func__);
        value = 0;
    }

    /*
     * Update VHT supported chan width:
     * if user setting is invalid, override it with
     * target capability
     */
    if ((value == eHT_CHANNEL_WIDTH_80MHZ &&
        !(cfg->supp_chan_width & eHT_CHANNEL_WIDTH_80MHZ)) ||
        (value == eHT_CHANNEL_WIDTH_160MHZ &&
        !(cfg->supp_chan_width & eHT_CHANNEL_WIDTH_160MHZ))) {
        u_int32_t width = eHT_CHANNEL_WIDTH_20MHZ;

        if (cfg->supp_chan_width & eHT_CHANNEL_WIDTH_160MHZ)
            width = eHT_CHANNEL_WIDTH_160MHZ;
        else if (cfg->supp_chan_width & eHT_CHANNEL_WIDTH_80MHZ)
            width = eHT_CHANNEL_WIDTH_80MHZ;

        status = ccmCfgSetInt(hdd_ctx->hHal,
                              WNI_CFG_VHT_SUPPORTED_CHAN_WIDTH_SET,
                              width, NULL, eANI_BOOLEAN_FALSE);

        if (status == eHAL_STATUS_FAILURE) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "%s: could not set VHT SUPPORTED CHAN WIDTH",
                      __func__);
        }
    }

    ccmCfgGetInt(hdd_ctx->hHal, WNI_CFG_VHT_BASIC_MCS_SET, &temp);
    temp = (temp & VHT_MCS_1x1) | pconfig->vhtRxMCS;

    if (pconfig->enable2x2)
            temp = (temp & VHT_MCS_2x2) | (pconfig->vhtRxMCS2x2 << 2);

    if (ccmCfgSetInt(hdd_ctx->hHal, WNI_CFG_VHT_BASIC_MCS_SET, temp, NULL,
                            eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE) {
        hddLog(LOGE, "Could not pass on WNI_CFG_VHT_BASIC_MCS_SET to CCM");
    }

    ccmCfgGetInt(hdd_ctx->hHal, WNI_CFG_VHT_RX_MCS_MAP, &temp);
    temp = (temp & VHT_MCS_1x1) | pconfig->vhtRxMCS;
    if (pconfig->enable2x2)
        temp = (temp & VHT_MCS_2x2) | (pconfig->vhtRxMCS2x2 << 2);

    if (ccmCfgSetInt(hdd_ctx->hHal, WNI_CFG_VHT_RX_MCS_MAP, temp, NULL,
                            eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE) {
        hddLog(LOGE, "Could not pass on WNI_CFG_VHT_RX_MCS_MAP to CCM");
    }

    ccmCfgGetInt(hdd_ctx->hHal, WNI_CFG_VHT_TX_MCS_MAP, &temp);
    temp = (temp & VHT_MCS_1x1) | pconfig->vhtTxMCS;
    if (pconfig->enable2x2)
        temp = (temp & VHT_MCS_2x2) | (pconfig->vhtTxMCS2x2 << 2);

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
                    "vhtRxMCS2x2 - %x temp - %u enable2x2 %d",
                    pconfig->vhtRxMCS2x2, temp, pconfig->enable2x2);

    if (ccmCfgSetInt(hdd_ctx->hHal, WNI_CFG_VHT_TX_MCS_MAP, temp, NULL,
                            eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE) {
        hddLog(LOGE, "Could not pass on WNI_CFG_VHT_TX_MCS_MAP to CCM");
    }
    /* Get the current RX LDPC setting */
    status = ccmCfgGetInt(hdd_ctx->hHal, WNI_CFG_VHT_LDPC_CODING_CAP, &value);

    if (status != eHAL_STATUS_SUCCESS) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: could not get VHT LDPC CODING CAP",
                  __func__);
        value = 0;
    }

    /* Set the LDPC capability */
    if (value && !cfg->vht_rx_ldpc) {
        status = ccmCfgSetInt(hdd_ctx->hHal,
                              WNI_CFG_VHT_LDPC_CODING_CAP,
                              cfg->vht_rx_ldpc, NULL,
                              eANI_BOOLEAN_FALSE);

        if (status == eHAL_STATUS_FAILURE) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "%s: could not set VHT LDPC CODING CAP to CCM",
                      __func__);
        }
    }

    /* Get current GI 80 value */
    status = ccmCfgGetInt(hdd_ctx->hHal, WNI_CFG_VHT_SHORT_GI_80MHZ, &value);

    if (status != eHAL_STATUS_SUCCESS) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: could not get SHORT GI 80MHZ",
                  __func__);
        value = 0;
    }

    /* set the Guard interval 80MHz */
    if (value && !cfg->vht_short_gi_80) {
        status = ccmCfgSetInt(hdd_ctx->hHal,
                              WNI_CFG_VHT_SHORT_GI_80MHZ,
                              cfg->vht_short_gi_80, NULL,
                              eANI_BOOLEAN_FALSE);

        if (status == eHAL_STATUS_FAILURE) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "%s: could not set SHORT GI 80MHZ to CCM",
                      __func__);
        }
    }

    /* Get current GI 160 value */
    status = ccmCfgGetInt(hdd_ctx->hHal,
                          WNI_CFG_VHT_SHORT_GI_160_AND_80_PLUS_80MHZ,
                          &value);

    if (status != eHAL_STATUS_SUCCESS) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: could not get SHORT GI 80 & 160",
                  __func__);
        value = 0;
    }

    /* set the Guard interval 160MHz */
    if (value && !cfg->vht_short_gi_160) {
        status = ccmCfgSetInt(hdd_ctx->hHal,
                              WNI_CFG_VHT_SHORT_GI_160_AND_80_PLUS_80MHZ,
                              cfg->vht_short_gi_160, NULL,
                              eANI_BOOLEAN_FALSE);

        if (status == eHAL_STATUS_FAILURE) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "%s: could not set SHORT GI 80 & 160 to CCM",
                      __func__);
        }
    }

    /* Get VHT TX STBC cap */
    status = ccmCfgGetInt(hdd_ctx->hHal, WNI_CFG_VHT_TXSTBC, &value);

    if (status != eHAL_STATUS_SUCCESS) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: could not get VHT TX STBC",
                  __func__);
        value = 0;
    }

    /* VHT TX STBC cap */
    if (value && !cfg->vht_tx_stbc) {
        status = ccmCfgSetInt(hdd_ctx->hHal, WNI_CFG_VHT_TXSTBC,
                              cfg->vht_tx_stbc, NULL,
                              eANI_BOOLEAN_FALSE);

        if (status == eHAL_STATUS_FAILURE) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "%s: could not set the VHT TX STBC to CCM",
                      __func__);
        }
    }

    /* Get VHT RX STBC cap */
    status = ccmCfgGetInt(hdd_ctx->hHal, WNI_CFG_VHT_RXSTBC, &value);

    if (status != eHAL_STATUS_SUCCESS) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: could not get VHT RX STBC",
                  __func__);
        value = 0;
    }

    /* VHT RX STBC cap */
    if (value && !cfg->vht_rx_stbc) {
        status = ccmCfgSetInt(hdd_ctx->hHal, WNI_CFG_VHT_RXSTBC,
                              cfg->vht_rx_stbc, NULL,
                              eANI_BOOLEAN_FALSE);

        if (status == eHAL_STATUS_FAILURE) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "%s: could not set the VHT RX STBC to CCM",
                      __func__);
        }
    }

    /* Get VHT SU Beamformer cap */
    status = ccmCfgGetInt(hdd_ctx->hHal, WNI_CFG_VHT_SU_BEAMFORMER_CAP,
                          &value);

    if (status != eHAL_STATUS_SUCCESS) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: could not get VHT SU BEAMFORMER CAP",
                  __func__);
        value = 0;
    }

    /* set VHT SU Beamformer cap */
    if (value && !cfg->vht_su_bformer) {
        status = ccmCfgSetInt(hdd_ctx->hHal,
                              WNI_CFG_VHT_SU_BEAMFORMER_CAP,
                              cfg->vht_su_bformer, NULL,
                              eANI_BOOLEAN_FALSE);

        if (status == eHAL_STATUS_FAILURE) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "%s: could not set VHT SU BEAMFORMER CAP",
                      __func__);
        }
    }

    /* check and update SU BEAMFORMEE capability*/
    if (pconfig->enableTxBF && !cfg->vht_su_bformee)
        pconfig->enableTxBF = cfg->vht_su_bformee;

    status = ccmCfgSetInt(hdd_ctx->hHal,
                          WNI_CFG_VHT_SU_BEAMFORMEE_CAP,
                          pconfig->enableTxBF, NULL,
                          eANI_BOOLEAN_FALSE);

    if (status == eHAL_STATUS_FAILURE) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                  "%s: could not set VHT SU BEAMFORMEE CAP",
                  __func__);
    }

    /* Get VHT MU Beamformer cap */
    status = ccmCfgGetInt(hdd_ctx->hHal, WNI_CFG_VHT_MU_BEAMFORMER_CAP,
                          &value);

    if (status != eHAL_STATUS_SUCCESS) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: could not get VHT MU BEAMFORMER CAP",
                  __func__);
        value = 0;
    }

    /* set VHT MU Beamformer cap */
    if (value && !cfg->vht_mu_bformer) {
        status = ccmCfgSetInt(hdd_ctx->hHal,
                              WNI_CFG_VHT_MU_BEAMFORMER_CAP,
                              cfg->vht_mu_bformer, NULL,
                              eANI_BOOLEAN_FALSE);

        if (status == eHAL_STATUS_FAILURE) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "%s: could not set the VHT MU BEAMFORMER CAP to CCM",
                      __func__);
        }
    }

    /* Get VHT MU Beamformee cap */
    status = ccmCfgGetInt(hdd_ctx->hHal, WNI_CFG_VHT_MU_BEAMFORMEE_CAP,
                          &value);

    if (status != eHAL_STATUS_SUCCESS) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: could not get VHT MU BEAMFORMEE CAP",
                  __func__);
        value = 0;
    }

    /* set VHT MU Beamformee cap */
    if (value && !cfg->vht_mu_bformee) {
        status = ccmCfgSetInt(hdd_ctx->hHal,
                              WNI_CFG_VHT_MU_BEAMFORMEE_CAP,
                              cfg->vht_mu_bformee, NULL,
                              eANI_BOOLEAN_FALSE);

        if (status == eHAL_STATUS_FAILURE) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "%s: could not set VHT MU BEAMFORMER CAP",
                      __func__);
        }
    }

    /* Get VHT MAX AMPDU Len exp */
    status = ccmCfgGetInt(hdd_ctx->hHal, WNI_CFG_VHT_AMPDU_LEN_EXPONENT,
                          &value);

    if (status != eHAL_STATUS_SUCCESS) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: could not get VHT AMPDU LEN",
                  __func__);
        value = 0;
    }

    /*
     * VHT max AMPDU len exp:
     * override if user configured value is too high
     * that the target cannot support.
     * Even though Rome publish ampdu_len=7, it can
     * only support 4 because of some h/w bug.
     */

    if (value > cfg->vht_max_ampdu_len_exp) {
        status = ccmCfgSetInt(hdd_ctx->hHal,
                              WNI_CFG_VHT_AMPDU_LEN_EXPONENT,
                              cfg->vht_max_ampdu_len_exp, NULL,
                              eANI_BOOLEAN_FALSE);

        if (status == eHAL_STATUS_FAILURE) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "%s: could not set the VHT AMPDU LEN EXP",
                      __func__);
        }
    }

    /* Get VHT TXOP PS CAP */
    status = ccmCfgGetInt(hdd_ctx->hHal, WNI_CFG_VHT_TXOP_PS, &value);

    if (status != eHAL_STATUS_SUCCESS) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: could not get VHT TXOP PS",
                  __func__);
        value = 0;
    }

    /* set VHT TXOP PS cap */
    if (value && !cfg->vht_txop_ps) {
        status = ccmCfgSetInt(hdd_ctx->hHal, WNI_CFG_VHT_TXOP_PS,
                              cfg->vht_txop_ps, NULL,
                              eANI_BOOLEAN_FALSE);

        if (status == eHAL_STATUS_FAILURE) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                      "%s: could not set the VHT TXOP PS",
                      __func__);
        }
    }
}
#endif  /* #ifdef WLAN_FEATURE_11AC */

void hdd_update_tgt_cfg(void *context, void *param)
{
    hdd_context_t *hdd_ctx = (hdd_context_t *)context;
    struct hdd_tgt_cfg *cfg = (struct hdd_tgt_cfg *)param;
    tANI_U8 temp_band_cap;

    /* first store the INI band capability */
    temp_band_cap = hdd_ctx->cfg_ini->nBandCapability;

    hdd_ctx->cfg_ini->nBandCapability = cfg->band_cap;

    /* now overwrite the target band capability with INI
       setting if INI setting is a subset */

    if ((hdd_ctx->cfg_ini->nBandCapability == eCSR_BAND_ALL) &&
        (temp_band_cap != eCSR_BAND_ALL))
        hdd_ctx->cfg_ini->nBandCapability = temp_band_cap;
    else if ((hdd_ctx->cfg_ini->nBandCapability != eCSR_BAND_ALL) &&
             (temp_band_cap != eCSR_BAND_ALL) &&
             (hdd_ctx->cfg_ini->nBandCapability != temp_band_cap)) {
        hddLog(VOS_TRACE_LEVEL_WARN,
               FL("ini BandCapability not supported by the target"));
    }

    if (!hdd_ctx->isLogpInProgress) {
        hdd_ctx->reg.reg_domain = cfg->reg_domain;
        hdd_ctx->reg.eeprom_rd_ext = cfg->eeprom_rd_ext;
    }

    /* This can be extended to other configurations like ht, vht cap... */

    if (!vos_is_macaddr_zero(&cfg->hw_macaddr))
    {
        hdd_update_macaddr(hdd_ctx->cfg_ini, cfg->hw_macaddr);
    }
    else {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: Invalid MAC passed from target, using MAC from ini file"
               MAC_ADDRESS_STR, __func__,
               MAC_ADDR_ARRAY(hdd_ctx->cfg_ini->intfMacAddr[0].bytes));
    }

    hdd_ctx->target_fw_version = cfg->target_fw_version;

    hdd_ctx->max_intf_count = cfg->max_intf_count;

#ifdef WLAN_FEATURE_LPSS
    hdd_ctx->lpss_support = cfg->lpss_support;
#endif

    wlan_hdd_set_egap_support(hdd_ctx, cfg);

    hdd_ctx->ap_arpns_support = cfg->ap_arpns_support;
    hdd_update_tgt_services(hdd_ctx, &cfg->services);
    if (hdd_ctx->per_band_chainmask_supp)
        hdd_update_chain_mask_vdev_nss(hdd_ctx, &cfg->services);
    else
        sme_set_vdev_nss(hdd_ctx->hHal, hdd_ctx->cfg_ini->enable2x2);

    hdd_update_tgt_ht_cap(hdd_ctx, &cfg->ht_cap);

#ifdef WLAN_FEATURE_11AC
    hdd_update_tgt_vht_cap(hdd_ctx, &cfg->vht_cap);
#endif  /* #ifdef WLAN_FEATURE_11AC */

    hdd_ctx->current_antenna_mode =
            hdd_is_supported_chain_mask_2x2(hdd_ctx) ?
            HDD_ANTENNA_MODE_2X2 : HDD_ANTENNA_MODE_1X1;
    hddLog(LOG1, FL("Current antenna mode: %d"),
           hdd_ctx->current_antenna_mode);
    hdd_ctx->cfg_ini->fine_time_meas_cap &= cfg->fine_time_measurement_cap;
    hddLog(LOG1, FL("fine_time_measurement_cap: 0x%x"),
             hdd_ctx->cfg_ini->fine_time_meas_cap);
    hdd_ctx->bpf_enabled = cfg->bpf_enabled;
    if (hdd_ctx->bpf_enabled)
             hdd_ctx->cfg_ini->maxWoWFilters = MAX_WOW_FILTERS;
    hdd_ctx->wmi_max_len = cfg->wmi_max_len;
}

/* This function is invoked in atomic context when a radar
 * is found on the SAP current operating channel and Data
 * Tx from netif has to be stopped to honor the DFS regulations.
 * Actions: Stop the netif Tx queues,Indicate Radar present
 * in HDD context for future usage.
 */
bool hdd_dfs_indicate_radar(void *context, void *param)
{
    hdd_context_t *pHddCtx= (hdd_context_t *)context;
    struct hdd_dfs_radar_ind *hdd_radar_event =
                              (struct hdd_dfs_radar_ind*)param;
    hdd_adapter_list_node_t *pAdapterNode = NULL,
                            *pNext = NULL;
    hdd_adapter_t *pAdapter;
    VOS_STATUS status;

    if (!pHddCtx || !hdd_radar_event || pHddCtx->cfg_ini->disableDFSChSwitch)
        return true;

    if (VOS_TRUE == hdd_radar_event->dfs_radar_status)
    {
        spin_lock_bh(&pHddCtx->dfs_lock);
        if (pHddCtx->dfs_radar_found)
        {
            /* Application already triggered channel switch
             * on current channel, so return here
             */
            spin_unlock_bh(&pHddCtx->dfs_lock);
            return false;
        }

        pHddCtx->dfs_radar_found = VOS_TRUE;
        spin_unlock_bh(&pHddCtx->dfs_lock);

        status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
        while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
        {
            pAdapter = pAdapterNode->pAdapter;
            if (WLAN_HDD_SOFTAP == pAdapter->device_mode ||
                WLAN_HDD_P2P_GO == pAdapter->device_mode)
            {
                WLAN_HDD_GET_AP_CTX_PTR(pAdapter)->dfs_cac_block_tx = VOS_TRUE;
            }

            status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
            pAdapterNode = pNext;
        }
    }

    return true;
}

/**---------------------------------------------------------------------------

  \brief hdd_is_valid_mac_address() - Validate MAC address

  This function validates whether the given MAC address is valid or not
  Expected MAC address is of the format XX:XX:XX:XX:XX:XX
  where X is the hexa decimal digit character and separated by ':'
  This algorithm works even if MAC address is not separated by ':'

  This code checks given input string mac contains exactly 12 hexadecimal digits.
  and a separator colon : appears in the input string only after
  an even number of hex digits.

  \param  - pMacAddr pointer to the input MAC address
  \return - 1 for valid and 0 for invalid

  --------------------------------------------------------------------------*/

v_BOOL_t hdd_is_valid_mac_address(const tANI_U8 *pMacAddr)
{
    int xdigit = 0;
    int separator = 0;
    while (*pMacAddr)
    {
        if (isxdigit(*pMacAddr))
        {
            xdigit++;
        }
        else if (':' == *pMacAddr)
        {
            if (0 == xdigit || ((xdigit / 2) - 1) != separator)
                break;

            ++separator;
        }
        else
        {
            /* Invalid MAC found */
            return 0;
        }
        ++pMacAddr;
    }
    return (xdigit == 12 && (separator == 5 || separator == 0));
}

/**
 * __hdd_open() - HDD Open function
 * @dev: pointer to net_device structure
 *
 * This is called in response to ifconfig up
 *
 * Return: 0 for success and error number for failure
 */
static int __hdd_open(struct net_device *dev)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_context_t *pHddCtx =  WLAN_HDD_GET_CTX(pAdapter);
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   v_BOOL_t in_standby = TRUE;

   MTRACE(vos_trace(VOS_MODULE_ID_HDD, TRACE_CODE_HDD_OPEN_REQUEST,
                    pAdapter->sessionId, pAdapter->device_mode));

   /* Don't validate for load/unload and logp as if we return
      failure we may endup in scan/connection related issues */
   if (NULL == pHddCtx || NULL == pHddCtx->cfg_ini) {
       hddLog(LOG1, FL("HDD context is Null"));
       return -ENODEV;
   }

   status = hdd_get_front_adapter (pHddCtx, &pAdapterNode);
   while ((NULL != pAdapterNode) && (VOS_STATUS_SUCCESS == status)) {
      if (test_bit(DEVICE_IFACE_OPENED, &pAdapterNode->pAdapter->event_flags)) {
         hddLog(LOG1, FL("chip already out of standby"));
         in_standby = FALSE;
         break;
      } else {
         status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
         pAdapterNode = pNext;
      }
   }

   if (TRUE == in_standby) {
       if (VOS_STATUS_SUCCESS != wlan_hdd_exit_lowpower(pHddCtx, pAdapter)) {
           hddLog(LOGE, FL("Failed to bring wlan out of power save"));
           return -EINVAL;
       }
   }

   set_bit(DEVICE_IFACE_OPENED, &pAdapter->event_flags);
   if (hdd_connIsConnected(WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))) {
       hddLog(LOG1, FL("Enabling Tx Queues"));
       /* Enable TX queues only when we are connected */
       netif_tx_start_all_queues(dev);
   }

   return 0;
}

/**
 * hdd_open() - Wrapper function for __hdd_open to protect it from SSR
 * @dev: pointer to net_device structure
 *
 * This is called in response to ifconfig up
 *
 * Return: 0 for success and error number for failure
 */
static int hdd_open(struct net_device *dev)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __hdd_open(dev);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**
 * __hdd_mon_open() - HDD monitor open
 * @dev: pointer to net_device
 *
 * Return: 0 for success and error number for failure
 */
static int __hdd_mon_open(struct net_device *dev)
{
	netif_start_queue(dev);
	return 0;
}

/**
 * hdd_mon_open() - SSR wrapper function for __hdd_mon_open
 * @dev: pointer to net_device
 *
 * Return: 0 for success and error number for failure
 */
static int hdd_mon_open(struct net_device *dev)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __hdd_mon_open(dev);
	vos_ssr_unprotect(__func__);

	return ret;
}

#ifdef MODULE
/**
 * wlan_hdd_stop_enter_lowpower() - Enter low power mode
 * @hdd_ctx:	HDD context
 *
 * For module, when all the interfaces are down, enter low power mode.
 */
static inline void wlan_hdd_stop_enter_lowpower(hdd_context_t *hdd_ctx)
{
	hddLog(VOS_TRACE_LEVEL_INFO,
			"%s: All Interfaces are Down entering standby",
			__func__);
	if (VOS_STATUS_SUCCESS != wlan_hdd_enter_lowpower(hdd_ctx)) {
		hddLog(VOS_TRACE_LEVEL_ERROR,
				"%s: Failed to put wlan in power save",
				__func__);
	}
}

/**
 * wlan_hdd_stop_can_enter_lowpower() - Enter low power mode
 * @adapter:	Adapter context
 *
 * Check if hardware can enter low power mode when all the interfaces are down.
 */
static inline int wlan_hdd_stop_can_enter_lowpower(hdd_adapter_t *adapter)
{
	/* SoftAP ifaces should never go in power save mode making
	 * sure same here.
	 */
	if ((WLAN_HDD_SOFTAP == adapter->device_mode) ||
			(WLAN_HDD_MONITOR == adapter->device_mode) ||
			(WLAN_HDD_P2P_GO == adapter->device_mode))
		return 0;

	return 1;
}
#else

/**
 * kickstart_driver_handler() - Work queue handler for kickstart_driver
 *
 * Use worker queue to exit if it is not possible to call kickstart_driver()
 * directly in the caller context like in interface down context
 */
static void kickstart_driver_handler(struct work_struct *work)
{
	hdd_driver_exit();
	wlan_hdd_inited = 0;
}

static DECLARE_WORK(kickstart_driver_work, kickstart_driver_handler);

/**
 * kickstart_driver() - Initialize and Clean-up driver
 * @load:	True: initialize, False: Clean-up driver
 *
 * Delayed driver initialization when driver is statically linked and Clean-up
 * when all the interfaces are down or any other condition which requires to
 * save power by bringing down hardware.
 * This routine is invoked when module parameter fwpath is modified from user
 * space to signal the initialization of the WLAN driver or when all the
 * interfaces are down and user space no longer need WLAN interfaces. Userspace
 * needs to write to fwpath again to get the WLAN interfaces
 *
 * Return: 0 on success, non zero on failure
 */
static int kickstart_driver(bool load)
{
	int ret_status;

	pr_info("%s: load: %d wlan_hdd_inited: %d, caller: %pf\n", __func__,
			load, wlan_hdd_inited, (void *)_RET_IP_);

	/* Make sure unload and load are synchronized */
	flush_work(&kickstart_driver_work);

	/* No-Op, If unload requested even though driver is not loaded */
	if (!load && !wlan_hdd_inited)
		return 0;

	/* Unload is requested */
	if (!load && wlan_hdd_inited) {
		schedule_work(&kickstart_driver_work);
		return 0;
	}

	if (!wlan_hdd_inited) {
		ret_status = hdd_driver_init();
		wlan_hdd_inited = ret_status ? 0 : 1;
		return ret_status;
	}

	hdd_driver_exit();

	msleep(200);

	ret_status = hdd_driver_init();
	wlan_hdd_inited = ret_status ? 0 : 1;
	return ret_status;
}

/**
 * wlan_hdd_stop_enter_lowpower() - Enter low power mode
 * @hdd_ctx:	HDD context
 *
 * For static driver, when all the interfaces are down, enter low power mode by
 * bringing down WLAN hardware.
 */
static inline void wlan_hdd_stop_enter_lowpower(hdd_context_t *hdd_ctx)
{
	kickstart_driver(false);
}

/**
 * wlan_hdd_stop_can_enter_lowpower() - Enter low power mode
 * @adapter:	Adapter context
 *
 * Check if hardware can enter low power mode when all the interfaces are down.
 * For static driver, hardware can enter low power mode for all types of
 * interfaces.
 */
static inline int wlan_hdd_stop_can_enter_lowpower(hdd_adapter_t *adapter)
{
	return 1;
}
#endif

/**
 * __hdd_stop() - HDD stop function
 * @dev: pointer to net_device structure
 *
 * This is called in response to ifconfig down
 *
 * Return: 0 for success and error number for failure
 */
static int __hdd_stop(struct net_device *dev)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_context_t *pHddCtx =  WLAN_HDD_GET_CTX(pAdapter);
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   v_BOOL_t enter_standby = TRUE;
   int ret;

   ENTER();

   MTRACE(vos_trace(VOS_MODULE_ID_HDD, TRACE_CODE_HDD_STOP_REQUEST,
                    pAdapter->sessionId, pAdapter->device_mode));

   ret = wlan_hdd_validate_context(pHddCtx);
   if (0 != ret)
       return ret;

   /* Nothing to be done if the interface is not opened */
   if (VOS_FALSE == test_bit(DEVICE_IFACE_OPENED, &pAdapter->event_flags))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: NETDEV Interface is not OPENED", __func__);
      return -ENODEV;
   }

   /* Make sure the interface is marked as closed */
   clear_bit(DEVICE_IFACE_OPENED, &pAdapter->event_flags);
   hddLog(VOS_TRACE_LEVEL_INFO, "%s: Disabling OS Tx queues", __func__);

   /* Disable TX on the interface, after this hard_start_xmit() will not
    * be called on that interface
    */
   hddLog(LOG1, FL("Disabling queues"));
   netif_tx_disable(pAdapter->dev);

   /* Mark the interface status as "down" for outside world */
   netif_carrier_off(pAdapter->dev);

   /* The interface is marked as down for outside world (aka kernel)
    * But the driver is pretty much alive inside. The driver needs to
    * tear down the existing connection on the netdev (session)
    * cleanup the data pipes and wait until the control plane is stabilized
    * for this interface. The call also needs to wait until the above
    * mentioned actions are completed before returning to the caller.
    * Notice that the hdd_stop_adapter is requested not to close the session
    * That is intentional to be able to scan if it is a STA/P2P interface
    */
   hdd_stop_adapter(pHddCtx, pAdapter, VOS_FALSE);

   /* DeInit the adapter. This ensures datapath cleanup as well */
   hdd_deinit_adapter(pHddCtx, pAdapter, true);

   /* SoftAP ifaces should never go in power save mode
      making sure same here. */
   if (!wlan_hdd_stop_can_enter_lowpower(pAdapter))
   {
      /* SoftAP mode, so return from here */
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s: In SAP MODE", __func__);
      EXIT();
      return 0;
   }

   /* Find if any iface is up. If any iface is up then can't put device to
    * sleep/power save mode
    */
   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
   while ( (NULL != pAdapterNode) && (VOS_STATUS_SUCCESS == status) )
   {
      if (test_bit(DEVICE_IFACE_OPENED, &pAdapterNode->pAdapter->event_flags))
      {
         hddLog(VOS_TRACE_LEVEL_INFO, "%s: Still other ifaces are up cannot "
                "put device to sleep", __func__);
         enter_standby = FALSE;
         break;
      }
      else
      {
         status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
         pAdapterNode = pNext;
      }
   }

   if (TRUE == enter_standby)
      wlan_hdd_stop_enter_lowpower(pHddCtx);

   EXIT();
   return 0;
}

/**
 * hdd_stop() - Wrapper function for __hdd_stop to protect it from SSR
 * @dev: pointer to net_device structure
 *
 * This is called in response to ifconfig down
 *
 * Return: 0 for success and error number for failure
 */
static int hdd_stop (struct net_device *dev)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __hdd_stop(dev);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**
 * __hdd_uninit() - HDD uninit function
 * @dev: pointer to net_device structure
 *
 * This is called during the netdev unregister to uninitialize all data
 * associated with the device
 *
 * Return: none
 */
static void __hdd_uninit(struct net_device *dev)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);

	ENTER();

	if (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic) {
		hddLog(LOGP, FL("Invalid magic"));
		return;
	}

	if (NULL == pAdapter->pHddCtx) {
		hddLog(LOGP, FL("NULL pHddCtx"));
		return;
	}

	if (dev != pAdapter->dev)
		hddLog(LOGP, FL("Invalid device reference"));

	hdd_deinit_adapter(pAdapter->pHddCtx, pAdapter, true);

	/* After uninit our adapter structure will no longer be valid */
	pAdapter->dev = NULL;
	pAdapter->magic = 0;

	EXIT();
}

/**
 * hdd_uninit() - Wrapper function to protect __hdd_uninit from SSR
 * @dev: pointer to net_device structure
 *
 * This is called during the netdev unregister to uninitialize all data
 * associated with the device
 *
 * Return: none
 */
static void hdd_uninit(struct net_device *dev)
{
	vos_ssr_protect(__func__);
	__hdd_uninit(dev);
	vos_ssr_unprotect(__func__);
}


/**---------------------------------------------------------------------------
     \brief hdd_full_pwr_cbk() - HDD full power callback function

      This is the function invoked by SME to inform the result of a full power
      request issued by HDD

     \param  - callback context - Pointer to cookie
               status - result of request

     \return - None

--------------------------------------------------------------------------*/
void hdd_full_pwr_cbk(void *callbackContext, eHalStatus status)
{
   hdd_context_t *pHddCtx = (hdd_context_t*)callbackContext;

   hddLog(VOS_TRACE_LEVEL_INFO_HIGH,"HDD full Power callback status = %d", status);
   if(&pHddCtx->full_pwr_comp_var)
   {
      complete(&pHddCtx->full_pwr_comp_var);
   }
}

static void hdd_tx_fail_ind_callback(v_U8_t *MacAddr, v_U8_t seqNo)
{
   int payload_len;
   struct sk_buff *skb;
   struct nlmsghdr *nlh;
   v_U8_t *data;

   payload_len = ETH_ALEN;

   if (0 == cesium_pid)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: cesium process not registered",
             __func__);
      return;
   }

   if ((skb = nlmsg_new(payload_len,GFP_ATOMIC)) == NULL)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: nlmsg_new() failed for msg size[%d]",
             __func__, NLMSG_SPACE(payload_len));
      return;
   }

   nlh = nlmsg_put(skb, cesium_pid, seqNo, 0, payload_len, NLM_F_REQUEST);

   if (NULL == nlh)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: nlmsg_put() failed for msg size[%d]",
             __func__, NLMSG_SPACE(payload_len));

      kfree_skb(skb);
      return;
   }

   data = nlmsg_data(nlh);
   memcpy(data, MacAddr, ETH_ALEN);

   if (nlmsg_unicast(cesium_nl_srv_sock, skb, cesium_pid) < 0)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: nlmsg_unicast() failed for msg size[%d]",
                                       __func__, NLMSG_SPACE(payload_len));
   }

   return;
}

/**---------------------------------------------------------------------------
     \brief hdd_ParseuserParams - return a pointer to the next argument

     \return - status

--------------------------------------------------------------------------*/
static int hdd_ParseUserParams(tANI_U8 *pValue, tANI_U8 **ppArg)
{
   tANI_U8 *pVal;

   pVal = strchr(pValue, ' ');

   if (NULL == pVal)
   {
      /* no argument remains */
      return -EINVAL;
   }
   else if (SPACE_ASCII_VALUE != *pVal)
   {
      /* no space after the current argument */
      return -EINVAL;
   }

   pVal++;

   /* remove empty spaces */
   while ((SPACE_ASCII_VALUE  == *pVal) && ('\0' !=  *pVal))
   {
      pVal++;
   }

   /* no argument followed by spaces */
   if ('\0' == *pVal)
   {
      return -EINVAL;
   }

   *ppArg = pVal;

   return 0;
}

/**----------------------------------------------------------------------------
     \brief hdd_ParseIBSSTXFailEventParams - Parse params for SETIBSSTXFAILEVENT

     \return - status

------------------------------------------------------------------------------*/
static int hdd_ParseIBSSTXFailEventParams(tANI_U8 *pValue,
                                          tANI_U8 *tx_fail_count,
                                          tANI_U16 *pid)
{
   tANI_U8 *param = NULL;
   int ret;

   ret = hdd_ParseUserParams(pValue, &param);

   if (0 == ret && NULL != param)
   {
      if (1 != sscanf(param, "%hhu", tx_fail_count))
      {
         ret = -EINVAL;
         goto done;
      }
   }
   else
   {
      goto done;
   }

   if (0 == *tx_fail_count)
   {
      *pid = 0;
      goto done;
   }

   pValue = param;
   pValue++;

   ret = hdd_ParseUserParams(pValue, &param);

   if (0 == ret)
   {
      if (1 != sscanf(param, "%hu", pid))
      {
         ret = -EINVAL;
         goto done;
      }
   }
   else
   {
      goto done;
   }

done:
   return ret;
}

static int hdd_open_cesium_nl_sock(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
   struct netlink_kernel_cfg cfg = {
          .groups = WLAN_NLINK_MCAST_GRP_ID,
          .input = NULL
          };
#endif
   int ret = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
   cesium_nl_srv_sock = netlink_kernel_create(&init_net, WLAN_NLINK_CESIUM,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0))
                                              THIS_MODULE,
#endif
                                              &cfg);
#else
   cesium_nl_srv_sock = netlink_kernel_create(&init_net, WLAN_NLINK_CESIUM,
                                        WLAN_NLINK_MCAST_GRP_ID, NULL, NULL, THIS_MODULE);
#endif

   if (cesium_nl_srv_sock == NULL)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "NLINK:  cesium netlink_kernel_create failed");
       ret = -ECONNREFUSED;
   }

   return ret;
}

static void hdd_close_cesium_nl_sock(void)
{
   if (NULL != cesium_nl_srv_sock)
   {
      netlink_kernel_release(cesium_nl_srv_sock);
      cesium_nl_srv_sock = NULL;
   }
}

/**---------------------------------------------------------------------------

  \brief hdd_get_cfg_file_size() -

   This function reads the configuration file using the request firmware
   API and returns the configuration file size.

  \param  - pCtx - Pointer to the adapter .
              - pFileName - Pointer to the file name.
              - pBufSize - Pointer to the buffer size.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

VOS_STATUS hdd_get_cfg_file_size(v_VOID_t *pCtx, char *pFileName, v_SIZE_t *pBufSize)
{
   int status;
   hdd_context_t *pHddCtx = (hdd_context_t*)pCtx;

   ENTER();

   status = request_firmware(&pHddCtx->fw, pFileName, pHddCtx->parent_dev);

   if(status || !pHddCtx->fw || !pHddCtx->fw->data) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: CFG download failed",__func__);
      status = VOS_STATUS_E_FAILURE;
   }
   else {
      *pBufSize = pHddCtx->fw->size;
      hddLog(VOS_TRACE_LEVEL_INFO, "%s: CFG size = %d", __func__, *pBufSize);
      release_firmware(pHddCtx->fw);
      pHddCtx->fw = NULL;
   }

   EXIT();
   return VOS_STATUS_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief hdd_read_cfg_file() -

   This function reads the configuration file using the request firmware
   API and returns the cfg data and the buffer size of the configuration file.

  \param  - pCtx - Pointer to the adapter .
              - pFileName - Pointer to the file name.
              - pBuffer - Pointer to the data buffer.
              - pBufSize - Pointer to the buffer size.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

VOS_STATUS hdd_read_cfg_file(v_VOID_t *pCtx, char *pFileName,
    v_VOID_t *pBuffer, v_SIZE_t *pBufSize)
{
   int status;
   hdd_context_t *pHddCtx = (hdd_context_t*)pCtx;

   ENTER();

   status = request_firmware(&pHddCtx->fw, pFileName, pHddCtx->parent_dev);

   if(status || !pHddCtx->fw || !pHddCtx->fw->data) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: CFG download failed",__func__);
      return VOS_STATUS_E_FAILURE;
   }
   else {
      if(*pBufSize != pHddCtx->fw->size) {
         hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Caller sets invalid CFG "
             "file size", __func__);
         release_firmware(pHddCtx->fw);
         pHddCtx->fw = NULL;
         return VOS_STATUS_E_FAILURE;
      }
        else {
         if(pBuffer) {
            vos_mem_copy(pBuffer,pHddCtx->fw->data,*pBufSize);
         }
         release_firmware(pHddCtx->fw);
         pHddCtx->fw = NULL;
        }
   }

   EXIT();

   return VOS_STATUS_SUCCESS;
}

/**
 * __hdd_set_mac_address() - HDD set mac address
 * @dev: pointer to net_device structure
 * @addr: Pointer to the sockaddr
 *
 * This function sets the user specified mac address using
 * the command ifconfig wlanX hw ether <mac adress>.
 *
 * Return: 0 for success.
 */
static int __hdd_set_mac_address(struct net_device *dev, void *addr)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx;
	struct sockaddr *psta_mac_addr = addr;
	int ret;

	ENTER();

	hdd_ctx = WLAN_HDD_GET_CTX(pAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	memcpy(&pAdapter->macAddressCurrent, psta_mac_addr->sa_data, ETH_ALEN);
	memcpy(dev->dev_addr, psta_mac_addr->sa_data, ETH_ALEN);

	EXIT();
	return 0;
}

/**
 * hdd_set_mac_address() - Wrapper function to protect __hdd_set_mac_address()
 *			function from SSR
 * @dev: pointer to net_device structure
 * @addr: Pointer to the sockaddr
 *
 * This function sets the user specified mac address using
 * the command ifconfig wlanX hw ether <mac adress>.
 *
 * Return: 0 for success.
 */
static int hdd_set_mac_address(struct net_device *dev, void *addr)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __hdd_set_mac_address(dev, addr);
	vos_ssr_unprotect(__func__);

	return ret;
}

tANI_U8* wlan_hdd_get_intf_addr(hdd_context_t* pHddCtx)
{
   int i;
   for ( i = 0; i < VOS_MAX_CONCURRENCY_PERSONA; i++)
   {
      if( 0 == ((pHddCtx->cfg_ini->intfAddrMask) & (1 << i)))
         break;
   }

   if( VOS_MAX_CONCURRENCY_PERSONA == i)
      return NULL;

   pHddCtx->cfg_ini->intfAddrMask |= (1 << i);
   return &pHddCtx->cfg_ini->intfMacAddr[i].bytes[0];
}

void wlan_hdd_release_intf_addr(hdd_context_t* pHddCtx, tANI_U8* releaseAddr)
{
   int i;
   for ( i = 0; i < VOS_MAX_CONCURRENCY_PERSONA; i++)
   {
      if ( !memcmp(releaseAddr, &pHddCtx->cfg_ini->intfMacAddr[i].bytes[0], 6) )
      {
         pHddCtx->cfg_ini->intfAddrMask &= ~(1 << i);
         break;
      }
   }
   return;
}

#ifdef WLAN_FEATURE_PACKET_FILTERING
/**
 * __hdd_set_multicast_list() - set the multicast address list
 * @dev: pointer to net_device
 *
 * Return: none
 */
static void __hdd_set_multicast_list(struct net_device *dev)
{
   static const uint8_t ipv6_router_solicitation[] =
                         {0x33, 0x33, 0x00, 0x00, 0x00, 0x02};
   hdd_adapter_t *pAdapter;
   hdd_context_t *pHddCtx;
   int mc_count;
   int i = 0;
   struct netdev_hw_addr *ha;

   ENTER();

   if (VOS_FTM_MODE == hdd_get_conparam())
      return;

   pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   if (0 != wlan_hdd_validate_context(pHddCtx))
      return;

   /* Delete already configured multicast address list */
   wlan_hdd_set_mc_addr_list(pAdapter, false);

   if (dev->flags & IFF_ALLMULTI)
   {
      hddLog(VOS_TRACE_LEVEL_INFO,
            "%s: allow all multicast frames", __func__);
      pAdapter->mc_addr_list.mc_cnt = 0;
   }
   else
   {
      mc_count = netdev_mc_count(dev);
      hddLog(VOS_TRACE_LEVEL_INFO,
            "%s: mc_count = %u", __func__, mc_count);
      if (mc_count > WLAN_HDD_MAX_MC_ADDR_LIST)
      {
         hddLog(VOS_TRACE_LEVEL_INFO,
               "%s: No free filter available; allow all multicast frames", __func__);
         pAdapter->mc_addr_list.mc_cnt = 0;
         return;
      }

      pAdapter->mc_addr_list.mc_cnt = mc_count;

      netdev_for_each_mc_addr(ha, dev) {
         if (i == mc_count)
            break;
         /*
          * Skip following addresses:
          * 1)IPv6 router solicitation address
          * 2)Any other address pattern if its set during RXFILTER REMOVE
          *   driver command based on addr_filter_pattern
          */
         if ((!memcmp(ha->addr, ipv6_router_solicitation, ETH_ALEN)) ||
             (pAdapter->addr_filter_pattern && (!memcmp(ha->addr,
                                 &pAdapter->addr_filter_pattern, 1)))) {
                hddLog(LOG1, FL("MC/BC filtering Skip addr ="MAC_ADDRESS_STR),
                     MAC_ADDR_ARRAY(ha->addr));

                pAdapter->mc_addr_list.mc_cnt--;
                continue;
         }
         memset(&(pAdapter->mc_addr_list.addr[i][0]), 0, ETH_ALEN);
         memcpy(&(pAdapter->mc_addr_list.addr[i][0]), ha->addr, ETH_ALEN);
         hddLog(VOS_TRACE_LEVEL_INFO, "%s: mlist[%d] = "MAC_ADDRESS_STR,
               __func__, i,
               MAC_ADDR_ARRAY(pAdapter->mc_addr_list.addr[i]));
         i++;
      }
   }

   /* Configure the updated multicast address list */
   wlan_hdd_set_mc_addr_list(pAdapter, true);

   EXIT();
   return;
}

/**
 * hdd_set_multicast_list() - SSR wrapper function for __hdd_set_multicast_list
 * @dev: pointer to net_device
 *
 * Return: none
 */
static void hdd_set_multicast_list(struct net_device *dev)
{
	vos_ssr_protect(__func__);
	__hdd_set_multicast_list(dev);
	vos_ssr_unprotect(__func__);
}
#endif

/**---------------------------------------------------------------------------

  \brief hdd_select_queue() -

   This function is registered with the Linux OS for network
   core to decide which queue to use first.

  \param  - dev - Pointer to the WLAN device.
              - skb - Pointer to OS packet (sk_buff).
  \return - ac, Queue Index/access category corresponding to UP in IP header

  --------------------------------------------------------------------------*/
static v_U16_t hdd_select_queue(struct net_device *dev,
                         struct sk_buff *skb
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0))
                         , void *accel_priv
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0))
                         , select_queue_fallback_t fallback
#endif
)
{
   return hdd_wmm_select_queue(dev, skb);
}

static struct net_device_ops wlan_drv_ops = {
      .ndo_open = hdd_open,
      .ndo_stop = hdd_stop,
      .ndo_uninit = hdd_uninit,
      .ndo_start_xmit = hdd_hard_start_xmit,
      .ndo_tx_timeout = hdd_tx_timeout,
      .ndo_get_stats = hdd_stats,
      .ndo_do_ioctl = hdd_ioctl,
      .ndo_set_mac_address = hdd_set_mac_address,
      .ndo_select_queue    = hdd_select_queue,
#ifdef WLAN_FEATURE_PACKET_FILTERING
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,1,0)) || defined(WITH_BACKPORTS)
      .ndo_set_rx_mode = hdd_set_multicast_list,
#else
      .ndo_set_multicast_list = hdd_set_multicast_list,
#endif //LINUX_VERSION_CODE
#endif
 };
 static struct net_device_ops wlan_mon_drv_ops = {
      .ndo_open = hdd_mon_open,
      .ndo_stop = hdd_stop,
      .ndo_uninit = hdd_uninit,
      .ndo_start_xmit = hdd_mon_hard_start_xmit,
      .ndo_tx_timeout = hdd_tx_timeout,
      .ndo_get_stats = hdd_stats,
      .ndo_do_ioctl = hdd_ioctl,
      .ndo_set_mac_address = hdd_set_mac_address,
};


void hdd_set_station_ops( struct net_device *pWlanDev )
{
      pWlanDev->netdev_ops = &wlan_drv_ops;
}

#ifdef FEATURE_RUNTIME_PM
/**
 * hdd_runtime_suspend_init() - API to initialize runtime pm context
 * @hdd_ctx: HDD Context
 *
 * The API initializes the context to prevent runtime pm for various use
 * cases like scan, roc, dfs.
 * This API can be extended to initialize the context to prevent runtime pm
 *
 * Return: void
 */
void hdd_runtime_suspend_init(hdd_context_t *hdd_ctx)
{
	struct hdd_runtime_pm_context *context = &hdd_ctx->runtime_context;

	context->scan = vos_runtime_pm_prevent_suspend_init("scan");
	context->roc = vos_runtime_pm_prevent_suspend_init("roc");
	context->dfs = vos_runtime_pm_prevent_suspend_init("dfs");
}

/**
 * hdd_runtime_suspend_deinit() - API to deinit runtime pm context
 * @hdd_ctx: HDD context
 *
 * The API deinit the context to prevent runtime pm.
 *
 * Return: void
 */
void hdd_runtime_suspend_deinit(hdd_context_t *hdd_ctx)
{
	struct hdd_runtime_pm_context *context = &hdd_ctx->runtime_context;

	vos_runtime_pm_prevent_suspend_deinit(context->scan);
	context->scan = NULL;
	vos_runtime_pm_prevent_suspend_deinit(context->roc);
	context->roc = NULL;
	vos_runtime_pm_prevent_suspend_deinit(context->dfs);
	context->dfs = NULL;
}

/**
 * hdd_adapter_runtime_suspend_init() - API to init runtime pm context/adapter
 * @adapter: Interface Adapter
 *
 * API is used to init the context to prevent runtime pm/adapter
 *
 * Return: void
 */
static void
hdd_adapter_runtime_suspend_init(hdd_adapter_t *adapter)
{
	struct hdd_adapter_pm_context *context = &adapter->runtime_context;

	context->connect = vos_runtime_pm_prevent_suspend_init("connect");
}

/**
 * hdd_adapter_runtime_suspend_denit() - API to deinit runtime pm/adapter
 * @adapter: Interface Adapter
 *
 * API is used to deinit the context to prevent runtime pm/adapter
 *
 * Return: void
 */
static void hdd_adapter_runtime_suspend_denit(hdd_adapter_t *adapter)
{
	struct hdd_adapter_pm_context *context = &adapter->runtime_context;

	vos_runtime_pm_prevent_suspend_deinit(context->connect);
	context->connect = NULL;
}

#else
void hdd_runtime_suspend_init(hdd_context_t *hdd_ctx) { }
void hdd_runtime_suspend_deinit(hdd_context_t *hdd_ctx) { }
static inline void
hdd_adapter_runtime_suspend_init(hdd_adapter_t *adapter) { }
static inline void
hdd_adapter_runtime_suspend_denit(hdd_adapter_t *adapter) { }
#endif

static hdd_adapter_t* hdd_alloc_station_adapter( hdd_context_t *pHddCtx, tSirMacAddr macAddr, const char* name )
{
   struct net_device *pWlanDev = NULL;
   hdd_adapter_t *pAdapter = NULL;
   /*
    * cfg80211 initialization and registration....
    */
   pWlanDev = alloc_netdev_mq(sizeof( hdd_adapter_t ),
                              name,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)) || defined(WITH_BACKPORTS)
                              NET_NAME_UNKNOWN,
#endif
                              ether_setup,
                              NUM_TX_QUEUES);

   if(pWlanDev != NULL)
   {

      //Save the pointer to the net_device in the HDD adapter
      pAdapter = (hdd_adapter_t*) netdev_priv( pWlanDev );

      vos_mem_zero( pAdapter, sizeof( hdd_adapter_t ) );

      pAdapter->dev = pWlanDev;
      pAdapter->pHddCtx = pHddCtx;
      pAdapter->magic = WLAN_HDD_ADAPTER_MAGIC;

      init_completion(&pAdapter->session_open_comp_var);
      init_completion(&pAdapter->smps_force_mode_comp_var);
      init_completion(&pAdapter->session_close_comp_var);
      init_completion(&pAdapter->disconnect_comp_var);
      init_completion(&pAdapter->linkup_event_var);
      init_completion(&pAdapter->cancel_rem_on_chan_var);
      init_completion(&pAdapter->rem_on_chan_ready_event);
      init_completion(&pAdapter->offchannel_tx_event);
      init_completion(&pAdapter->tx_action_cnf_event);
#ifdef FEATURE_WLAN_TDLS
      init_completion(&pAdapter->tdls_add_station_comp);
      init_completion(&pAdapter->tdls_del_station_comp);
      init_completion(&pAdapter->tdls_mgmt_comp);
      init_completion(&pAdapter->tdls_link_establish_req_comp);
#endif

      init_completion(&pAdapter->ibss_peer_info_comp);
      init_completion(&pHddCtx->mc_sus_event_var);
      init_completion(&pHddCtx->tx_sus_event_var);
      init_completion(&pHddCtx->rx_sus_event_var);
      init_completion(&pHddCtx->ready_to_suspend);
      init_completion(&pAdapter->ula_complete);
      init_completion(&pAdapter->change_country_code);

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
      init_completion(&pHddCtx->ready_to_extwow);
#endif

      init_completion(&pAdapter->scan_info.scan_req_completion_event);
      init_completion(&pAdapter->scan_info.abortscan_event_var);

      vos_event_init(&pAdapter->scan_info.scan_finished_event);
      pAdapter->scan_info.scan_pending_option = WEXT_SCAN_PENDING_GIVEUP;

      pAdapter->offloads_configured = FALSE;
      pAdapter->isLinkUpSvcNeeded = FALSE;
      pAdapter->higherDtimTransition = eANI_BOOLEAN_TRUE;
      //Init the net_device structure
      strlcpy(pWlanDev->name, name, IFNAMSIZ);

      vos_mem_copy(pWlanDev->dev_addr, (void *)macAddr, sizeof(tSirMacAddr));
      vos_mem_copy( pAdapter->macAddressCurrent.bytes, macAddr, sizeof(tSirMacAddr));
      pWlanDev->watchdog_timeo = HDD_TX_TIMEOUT;
      /*
       * kernel will consume ethernet header length buffer for hard_header,
       * so just reserve it
       */
      hdd_set_needed_headroom(pWlanDev, pWlanDev->hard_header_len);
      pWlanDev->hard_header_len += HDD_HW_NEEDED_HEADROOM;

      if (pHddCtx->cfg_ini->enableIPChecksumOffload)
         pWlanDev->features |= NETIF_F_HW_CSUM;
      else if (pHddCtx->cfg_ini->enableTCPChkSumOffld)
         pWlanDev->features |= NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM;
         pWlanDev->features |= NETIF_F_RXCSUM;
      hdd_set_station_ops( pAdapter->dev );

      pWlanDev->destructor = free_netdev;
      pWlanDev->ieee80211_ptr = &pAdapter->wdev ;
      pWlanDev->tx_queue_len = HDD_NETDEV_TX_QUEUE_LEN;
      pAdapter->wdev.wiphy = pHddCtx->wiphy;
      pAdapter->wdev.netdev =  pWlanDev;
      /* set pWlanDev's parent to underlying device */
      SET_NETDEV_DEV(pWlanDev, pHddCtx->parent_dev);
      hdd_wmm_init( pAdapter );
      hdd_adapter_runtime_suspend_init(pAdapter);
   }

   return pAdapter;
}

VOS_STATUS hdd_register_interface( hdd_adapter_t *pAdapter, tANI_U8 rtnl_lock_held )
{
   struct net_device *pWlanDev = pAdapter->dev;
   //hdd_station_ctx_t *pHddStaCtx = &pAdapter->sessionCtx.station;
   //hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX( pAdapter );
   //eHalStatus halStatus = eHAL_STATUS_SUCCESS;

   if( rtnl_lock_held )
   {
     if (strnchr(pWlanDev->name, strlen(pWlanDev->name), '%')) {
         if( dev_alloc_name(pWlanDev, pWlanDev->name) < 0 )
         {
            hddLog(VOS_TRACE_LEVEL_ERROR,"%s:Failed:dev_alloc_name",__func__);
            return VOS_STATUS_E_FAILURE;
         }
      }
      if (register_netdevice(pWlanDev))
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,"%s:Failed:register_netdev",__func__);
         return VOS_STATUS_E_FAILURE;
      }
   }
   else
   {
      if(register_netdev(pWlanDev))
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Failed:register_netdev",__func__);
         return VOS_STATUS_E_FAILURE;
      }
   }
   set_bit(NET_DEVICE_REGISTERED, &pAdapter->event_flags);

   return VOS_STATUS_SUCCESS;
}

static eHalStatus hdd_smeCloseSessionCallback(void *pContext)
{
   hdd_adapter_t *pAdapter = pContext;

   if (NULL == pAdapter)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: NULL pAdapter", __func__);
      return eHAL_STATUS_INVALID_PARAMETER;
   }

   if (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Invalid magic", __func__);
      return eHAL_STATUS_NOT_INITIALIZED;
   }

   clear_bit(SME_SESSION_OPENED, &pAdapter->event_flags);

#if !defined (CONFIG_CNSS) && \
    !defined (WLAN_OPEN_SOURCE)
   /* need to make sure all of our scheduled work has completed.
    * This callback is called from MC thread context, so it is safe to
    * to call below flush work queue API from here.
    *
    * Even though this is called from MC thread context, if there is a faulty
    * work item in the system, that can hang this call forever.  So flushing
    * this global work queue is not safe; and now we make sure that
    * individual work queues are stopped correctly. But the cancel work queue
    * is a GPL only API, so the proprietary  version of the driver would still
    * rely on the global work queue flush.
    */
   flush_scheduled_work();
#endif

   /* We can be blocked while waiting for scheduled work to be
    * flushed, and the adapter structure can potentially be freed, in
    * which case the magic will have been reset.  So make sure the
    * magic is still good, and hence the adapter structure is still
    * valid, before signalling completion */
   if (WLAN_HDD_ADAPTER_MAGIC == pAdapter->magic)
   {
      complete(&pAdapter->session_close_comp_var);
   }

   return eHAL_STATUS_SUCCESS;
}

VOS_STATUS hdd_init_station_mode( hdd_adapter_t *pAdapter )
{
   struct net_device *pWlanDev = pAdapter->dev;
   hdd_station_ctx_t *pHddStaCtx = &pAdapter->sessionCtx.station;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX( pAdapter );
   eHalStatus halStatus = eHAL_STATUS_SUCCESS;
   VOS_STATUS status = VOS_STATUS_E_FAILURE;
   tANI_U32 type, subType;
   unsigned long rc;
   int ret_val;

   INIT_COMPLETION(pAdapter->session_open_comp_var);
   sme_SetCurrDeviceMode(pHddCtx->hHal, pAdapter->device_mode);
   sme_set_pdev_ht_vht_ies(pHddCtx->hHal, pHddCtx->cfg_ini->enable2x2);
   status = vos_get_vdev_types(pAdapter->device_mode, &type, &subType);
   if (VOS_STATUS_SUCCESS != status)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "failed to get vdev type");
      goto error_sme_open;
   }
   //Open a SME session for future operation
   halStatus = sme_OpenSession( pHddCtx->hHal, hdd_smeRoamCallback, pAdapter,
         (tANI_U8 *)&pAdapter->macAddressCurrent, &pAdapter->sessionId,
         type, subType);
   if ( !HAL_STATUS_SUCCESS( halStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,
             "sme_OpenSession() failed with status code %08d [x%08x]",
                                                 halStatus, halStatus );
      status = VOS_STATUS_E_FAILURE;
      goto error_sme_open;
   }

   //Block on a completion variable. Can't wait forever though.
   rc = wait_for_completion_timeout(
                        &pAdapter->session_open_comp_var,
                        msecs_to_jiffies(WLAN_WAIT_TIME_SESSIONOPENCLOSE));
   if (!rc) {
      hddLog(VOS_TRACE_LEVEL_FATAL,
             FL("Session is not opened within timeout period code %ld"),
             rc );
      status = VOS_STATUS_E_FAILURE;
      goto error_sme_open;
   }

   // Register wireless extensions
   if( eHAL_STATUS_SUCCESS !=  (halStatus = hdd_register_wext(pWlanDev)))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,
              "hdd_register_wext() failed with status code %08d [x%08x]",
                                                   halStatus, halStatus );
      status = VOS_STATUS_E_FAILURE;
      goto error_register_wext;
   }

   //Set the Connection State to Not Connected
   hddLog(VOS_TRACE_LEVEL_INFO,
            "%s: Set HDD connState to eConnectionState_NotConnected",
                   __func__);
   pHddStaCtx->conn_info.connState = eConnectionState_NotConnected;

   //Set the default operation channel
   pHddStaCtx->conn_info.operationChannel = pHddCtx->cfg_ini->OperatingChannel;

   /* Make the default Auth Type as OPEN*/
   pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_OPEN_SYSTEM;

   if( VOS_STATUS_SUCCESS != ( status = hdd_init_tx_rx( pAdapter ) ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,
            "hdd_init_tx_rx() failed with status code %08d [x%08x]",
                            status, status );
      goto error_init_txrx;
   }

   set_bit(INIT_TX_RX_SUCCESS, &pAdapter->event_flags);

   if( VOS_STATUS_SUCCESS != ( status = hdd_wmm_adapter_init( pAdapter ) ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,
            "hdd_wmm_adapter_init() failed with status code %08d [x%08x]",
                            status, status );
      goto error_wmm_init;
   }

   set_bit(WMM_INIT_DONE, &pAdapter->event_flags);

   ret_val = process_wma_set_command((int)pAdapter->sessionId,
                         (int)WMI_PDEV_PARAM_BURST_ENABLE,
                         (int)pHddCtx->cfg_ini->enableSifsBurst,
                         PDEV_CMD);

   if (0 != ret_val) {
       hddLog(VOS_TRACE_LEVEL_ERROR,
                   "%s: WMI_PDEV_PARAM_BURST_ENABLE set failed %d",
                   __func__, ret_val);
   }

#ifdef FEATURE_WLAN_TDLS
   if(0 != wlan_hdd_tdls_init(pAdapter))
   {
       status = VOS_STATUS_E_FAILURE;
       hddLog(VOS_TRACE_LEVEL_ERROR,"%s: wlan_hdd_tdls_init failed",__func__);
       goto error_tdls_init;
   }
   set_bit(TDLS_INIT_DONE, &pAdapter->event_flags);
#endif

   return VOS_STATUS_SUCCESS;

#ifdef FEATURE_WLAN_TDLS
error_tdls_init:
   clear_bit(WMM_INIT_DONE, &pAdapter->event_flags);
   hdd_wmm_adapter_close(pAdapter);
#endif
error_wmm_init:
   clear_bit(INIT_TX_RX_SUCCESS, &pAdapter->event_flags);
   hdd_deinit_tx_rx(pAdapter);
error_init_txrx:
   hdd_UnregisterWext(pWlanDev);
error_register_wext:
   if (test_bit(SME_SESSION_OPENED, &pAdapter->event_flags))
   {
      INIT_COMPLETION(pAdapter->session_close_comp_var);
      if (eHAL_STATUS_SUCCESS == sme_CloseSession(pHddCtx->hHal,
                                    pAdapter->sessionId,
                                    hdd_smeCloseSessionCallback, pAdapter))
      {
         unsigned long rc;

         //Block on a completion variable. Can't wait forever though.
         rc = wait_for_completion_timeout(
                          &pAdapter->session_close_comp_var,
                          msecs_to_jiffies(WLAN_WAIT_TIME_SESSIONOPENCLOSE));
         if (rc <= 0)
            hddLog(VOS_TRACE_LEVEL_ERROR,
                   FL("Session is not opened within timeout period code %ld"),
                   rc);
      }
   }
error_sme_open:
   return status;
}

void hdd_cleanup_actionframe( hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter )
{
   hdd_cfg80211_state_t *cfgState;

   cfgState = WLAN_HDD_GET_CFG_STATE_PTR( pAdapter );

   if( NULL != cfgState->buf )
   {
      unsigned long rc;
      INIT_COMPLETION(pAdapter->tx_action_cnf_event);
      rc = wait_for_completion_timeout(
                     &pAdapter->tx_action_cnf_event,
                     msecs_to_jiffies(ACTION_FRAME_TX_TIMEOUT));
      if (!rc)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s HDD Wait for Action Confirmation Failed!!",
                   __func__);
         /* Inform tx status as FAILURE to upper layer and free
          * cfgState->buf */
         hdd_sendActionCnf( pAdapter, FALSE );
      }
   }
   return;
}

void hdd_deinit_adapter(hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter,
                        bool rtnl_held)
{
   ENTER();
   switch ( pAdapter->device_mode )
   {
      case WLAN_HDD_INFRA_STATION:
      case WLAN_HDD_P2P_CLIENT:
      case WLAN_HDD_P2P_DEVICE:
      {
         if(test_bit(INIT_TX_RX_SUCCESS, &pAdapter->event_flags))
         {
            hdd_deinit_tx_rx( pAdapter );
            clear_bit(INIT_TX_RX_SUCCESS, &pAdapter->event_flags);
         }

         if(test_bit(WMM_INIT_DONE, &pAdapter->event_flags))
         {
            hdd_wmm_adapter_close( pAdapter );
            clear_bit(WMM_INIT_DONE, &pAdapter->event_flags);
         }

         hdd_cleanup_actionframe(pHddCtx, pAdapter);
         wlan_hdd_tdls_exit(pAdapter);
         break;
      }

      case WLAN_HDD_SOFTAP:
      case WLAN_HDD_P2P_GO:
      {

         if (test_bit(WMM_INIT_DONE, &pAdapter->event_flags))
         {
            hdd_wmm_adapter_close( pAdapter );
            clear_bit(WMM_INIT_DONE, &pAdapter->event_flags);
         }

         hdd_cleanup_actionframe(pHddCtx, pAdapter);

         hdd_unregister_hostapd(pAdapter, rtnl_held);

         // set con_mode to STA only when no SAP concurrency mode
         if (!(hdd_get_concurrency_mode() & (VOS_SAP | VOS_P2P_GO)))
             hdd_set_conparam( 0 );
         wlan_hdd_set_monitor_tx_adapter( WLAN_HDD_GET_CTX(pAdapter), NULL );
         break;
      }

      case WLAN_HDD_MONITOR:
      {
          hdd_adapter_t* pAdapterforTx = pAdapter->sessionCtx.monitor.pAdapterForTx;
         if(test_bit(INIT_TX_RX_SUCCESS, &pAdapter->event_flags))
         {
            hdd_deinit_tx_rx( pAdapter );
            clear_bit(INIT_TX_RX_SUCCESS, &pAdapter->event_flags);
         }
         if(NULL != pAdapterforTx)
         {
            hdd_cleanup_actionframe(pHddCtx, pAdapterforTx);
         }
         break;
      }


      default:
      break;
   }

   EXIT();
}

void hdd_cleanup_adapter(hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter,
                         tANI_U8 rtnl_held)
{
   struct net_device *pWlanDev = NULL;

   if (pAdapter)
      pWlanDev = pAdapter->dev;
   else {
      hddLog(LOGE, FL("pAdapter is Null"));
      return;
   }

    hdd_adapter_runtime_suspend_denit(pAdapter);
   /* The adapter is marked as closed. When hdd_wlan_exit() call returns,
    * the driver is almost closed and cannot handle either control
    * messages or data. However, unregister_netdevice() call above will
    * eventually invoke hdd_stop (ndo_close) driver callback, which attempts
    * to close the active connections (basically excites control path) which
    * is not right. Setting this flag helps hdd_stop() to recognize that
    * the interface is closed and restricts any operations on that
    */
   clear_bit(DEVICE_IFACE_OPENED, &pAdapter->event_flags);

   if (test_bit(NET_DEVICE_REGISTERED, &pAdapter->event_flags)) {
      if (rtnl_held) {
         unregister_netdevice(pWlanDev);
      } else {
         unregister_netdev(pWlanDev);
      }
      /* Note that the pAdapter is no longer valid at this point
         since the memory has been reclaimed */
   }
}

void hdd_set_pwrparams(hdd_context_t *pHddCtx)
{
   VOS_STATUS status;
   hdd_adapter_t *pAdapter = NULL;
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;

   status =  hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   /*loop through all adapters.*/
   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
       pAdapter = pAdapterNode->pAdapter;
       if ( (WLAN_HDD_INFRA_STATION != pAdapter->device_mode)
         && (WLAN_HDD_P2P_CLIENT != pAdapter->device_mode) )

       {  // we skip this registration for modes other than STA and P2P client modes.
           status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
           pAdapterNode = pNext;
           continue;
       }

       //Apply Dynamic DTIM For P2P
       //Only if ignoreDynamicDtimInP2pMode is not set in ini
      if ((pHddCtx->cfg_ini->enableDynamicDTIM ||
           pHddCtx->cfg_ini->enableModulatedDTIM) &&
          ((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) ||
          ((WLAN_HDD_P2P_CLIENT == pAdapter->device_mode) &&
          !(pHddCtx->cfg_ini->ignoreDynamicDtimInP2pMode))) &&
          (eANI_BOOLEAN_TRUE == pAdapter->higherDtimTransition) &&
          (eConnectionState_Associated ==
          (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState) &&
          (pHddCtx->cfg_ini->fIsBmpsEnabled))
      {
           tSirSetPowerParamsReq powerRequest = { 0 };

           powerRequest.uIgnoreDTIM = 1;
           powerRequest.uMaxLIModulatedDTIM = pHddCtx->cfg_ini->fMaxLIModulatedDTIM;

           if (pHddCtx->cfg_ini->enableModulatedDTIM)
           {
               powerRequest.uDTIMPeriod = pHddCtx->cfg_ini->enableModulatedDTIM;
               powerRequest.uListenInterval = pHddCtx->hdd_actual_LI_value;
           }
           else
           {
               powerRequest.uListenInterval = pHddCtx->cfg_ini->enableDynamicDTIM;
           }

           /* Update ignoreDTIM and ListedInterval in CFG to remain at the DTIM
            * specified during Enter/Exit BMPS when LCD off*/
            ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_IGNORE_DTIM, powerRequest.uIgnoreDTIM,
                       NULL, eANI_BOOLEAN_FALSE);
            ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_LISTEN_INTERVAL, powerRequest.uListenInterval,
                       NULL, eANI_BOOLEAN_FALSE);

           /* switch to the DTIM specified in cfg.ini */
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "Switch to DTIM %d", powerRequest.uListenInterval);
            sme_SetPowerParams( pHddCtx->hHal, &powerRequest, TRUE);
            break;

      }

      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
    }
}

void hdd_reset_pwrparams(hdd_context_t *pHddCtx)
{
   /*Switch back to DTIM 1*/
   tSirSetPowerParamsReq powerRequest = { 0 };

   powerRequest.uIgnoreDTIM = pHddCtx->hdd_actual_ignore_DTIM_value;
   powerRequest.uListenInterval = pHddCtx->hdd_actual_LI_value;
   powerRequest.uMaxLIModulatedDTIM = pHddCtx->cfg_ini->fMaxLIModulatedDTIM;

   /* Update ignoreDTIM and ListedInterval in CFG with default values */
   ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_IGNORE_DTIM, powerRequest.uIgnoreDTIM,
                    NULL, eANI_BOOLEAN_FALSE);
   ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_LISTEN_INTERVAL, powerRequest.uListenInterval,
                    NULL, eANI_BOOLEAN_FALSE);

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                  "Switch to DTIM%d",powerRequest.uListenInterval);
   sme_SetPowerParams( pHddCtx->hHal, &powerRequest, TRUE);

}

VOS_STATUS hdd_enable_bmps_imps(hdd_context_t *pHddCtx)
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;

   if (0 != wlan_hdd_validate_context(pHddCtx)) {
       hddLog(LOGE, FL("HDD context is not valid"));
       return VOS_STATUS_E_PERM;
   }

   if(pHddCtx->cfg_ini->fIsBmpsEnabled)
   {
      sme_EnablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);
   }

   if(pHddCtx->cfg_ini->fIsAutoBmpsTimerEnabled)
   {
      sme_StartAutoBmpsTimer(pHddCtx->hHal);
   }

   if (pHddCtx->cfg_ini->fIsImpsEnabled)
   {
      sme_EnablePowerSave (pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
   }

   return status;
}

VOS_STATUS hdd_disable_bmps_imps(hdd_context_t *pHddCtx, tANI_U8 session_type)
{
   hdd_adapter_t *pAdapter = NULL;
   eHalStatus halStatus;
   VOS_STATUS status = VOS_STATUS_E_INVAL;
   v_BOOL_t disableBmps = FALSE;
   v_BOOL_t disableImps = FALSE;

   switch(session_type)
   {
       case WLAN_HDD_INFRA_STATION:
       case WLAN_HDD_SOFTAP:
       case WLAN_HDD_P2P_CLIENT:
       case WLAN_HDD_P2P_GO:
          //Exit BMPS -> Is Sta/P2P Client is already connected
          pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_INFRA_STATION);
          if((NULL != pAdapter)&&
              hdd_connIsConnected( WLAN_HDD_GET_STATION_CTX_PTR(pAdapter)))
          {
             disableBmps = TRUE;
          }

          pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_P2P_CLIENT);
          if((NULL != pAdapter)&&
              hdd_connIsConnected( WLAN_HDD_GET_STATION_CTX_PTR(pAdapter)))
          {
             disableBmps = TRUE;
          }

          //Exit both Bmps and Imps incase of Go/SAP Mode
          if((WLAN_HDD_SOFTAP == session_type) ||
              (WLAN_HDD_P2P_GO == session_type))
          {
             disableBmps = TRUE;
             disableImps = TRUE;
          }

          if(TRUE == disableImps)
          {
             if (pHddCtx->cfg_ini->fIsImpsEnabled)
             {
                sme_DisablePowerSave (pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
             }
          }

          if(TRUE == disableBmps)
          {
             if(pHddCtx->cfg_ini->fIsBmpsEnabled)
             {
                 halStatus = sme_DisablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);

                 if(eHAL_STATUS_SUCCESS != halStatus)
                 {
                    status = VOS_STATUS_E_FAILURE;
                    hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Fail to Disable Power Save", __func__);
                    VOS_ASSERT(0);
                    return status;
                 }
              }

              if(pHddCtx->cfg_ini->fIsAutoBmpsTimerEnabled)
              {
                 halStatus = sme_StopAutoBmpsTimer(pHddCtx->hHal);

                 if(eHAL_STATUS_SUCCESS != halStatus)
                 {
                    status = VOS_STATUS_E_FAILURE;
                    hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Fail to Stop Auto Bmps Timer", __func__);
                    VOS_ASSERT(0);
                    return status;
                 }
              }
          }

          if((TRUE == disableBmps) ||
              (TRUE == disableImps))
          {
              /* Now, get the chip into Full Power now */
              INIT_COMPLETION(pHddCtx->full_pwr_comp_var);
              halStatus = sme_RequestFullPower(pHddCtx->hHal, hdd_full_pwr_cbk,
                                   pHddCtx, eSME_FULL_PWR_NEEDED_BY_HDD);

              if(halStatus != eHAL_STATUS_SUCCESS)
              {
                 if(halStatus == eHAL_STATUS_PMC_PENDING)
                 {
                    unsigned long rc;
                    //Block on a completion variable. Can't wait forever though
                    rc = wait_for_completion_timeout(
                                   &pHddCtx->full_pwr_comp_var,
                                   msecs_to_jiffies(1000));
                    if (!rc) {
                       hddLog(VOS_TRACE_LEVEL_ERROR,
                              "%s: wait on full_pwr_comp_var failed",
                              __func__);
}
                 }
                 else
                 {
                    status = VOS_STATUS_E_FAILURE;
                    hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Request for Full Power failed", __func__);
                    VOS_ASSERT(0);
                    return status;
                 }
              }

              status = VOS_STATUS_SUCCESS;
          }

          break;
   }
   return status;
}

VOS_STATUS hdd_check_for_existing_macaddr( hdd_context_t *pHddCtx,
                                           tSirMacAddr macAddr )
{
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
    hdd_adapter_t *pAdapter;
    VOS_STATUS status;

    status = hdd_get_front_adapter(pHddCtx, &pAdapterNode);

    while (NULL != pAdapterNode && VOS_STATUS_SUCCESS == status) {
        pAdapter = pAdapterNode->pAdapter;

        if (pAdapter && vos_mem_compare(pAdapter->macAddressCurrent.bytes,
                                         macAddr, sizeof(tSirMacAddr))) {
            return VOS_STATUS_E_FAILURE;
        }
        status = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pNext);
        pAdapterNode = pNext;
    }

    return VOS_STATUS_SUCCESS;
}

hdd_adapter_t* hdd_open_adapter( hdd_context_t *pHddCtx, tANI_U8 session_type,
                                 const char *iface_name, tSirMacAddr macAddr,
                                 tANI_U8 rtnl_held )
{
   hdd_adapter_t *pAdapter = NULL;
   hdd_adapter_list_node_t *pHddAdapterNode = NULL;
   VOS_STATUS status = VOS_STATUS_E_FAILURE;
   VOS_STATUS exitbmpsStatus = VOS_STATUS_E_FAILURE;
   hdd_cfg80211_state_t *cfgState;
   int ret;

   hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: iface =%s type = %d\n", __func__,
                                      iface_name, session_type);

   if (pHddCtx->current_intf_count >= pHddCtx->max_intf_count){
        /* Max limit reached on the number of vdevs configured by the host.
         *  Return error
         */
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: Unable to add virtual intf: currentVdevCnt=%d,hostConfiguredVdevCnt=%d",
                 __func__,pHddCtx->current_intf_count, pHddCtx->max_intf_count);
        return NULL;
   }

   if(macAddr == NULL)
   {
         /* Not received valid macAddr */
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s:Unable to add virtual intf: Not able to get"
                             "valid mac address",__func__);
         return NULL;
   }

   status = hdd_check_for_existing_macaddr(pHddCtx, macAddr);
   if (VOS_STATUS_E_FAILURE == status) {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Duplicate MAC addr: "MAC_ADDRESS_STR" already exists",
                   __func__, MAC_ADDR_ARRAY(macAddr));
         return NULL;
   }

   /*
    * If Powersave Offload is enabled
    * Fw will take care incase of concurrency
    */
   if(!pHddCtx->cfg_ini->enablePowersaveOffload)
   {
      //Disable BMPS incase of Concurrency
      exitbmpsStatus = hdd_disable_bmps_imps(pHddCtx, session_type);

      if(VOS_STATUS_E_FAILURE == exitbmpsStatus)
      {
         //Fail to Exit BMPS
         hddLog(VOS_TRACE_LEVEL_ERROR, FL("Fail to Exit BMPS"));
         VOS_ASSERT(0);
         return NULL;
      }
   }

   switch(session_type)
   {
      case WLAN_HDD_INFRA_STATION:
         /* Reset locally administered bit if the device mode is STA */
         WLAN_HDD_RESET_LOCALLY_ADMINISTERED_BIT(macAddr);
         /* fall through */
      case WLAN_HDD_P2P_CLIENT:
      case WLAN_HDD_P2P_DEVICE:
      case WLAN_HDD_OCB:
      {
         pAdapter = hdd_alloc_station_adapter( pHddCtx, macAddr, iface_name );

         if( NULL == pAdapter )
         {
            hddLog(VOS_TRACE_LEVEL_FATAL,
                   FL("failed to allocate adapter for session %d"), session_type);
            return NULL;
          }

         if (session_type == WLAN_HDD_P2P_CLIENT)
            pAdapter->wdev.iftype = NL80211_IFTYPE_P2P_CLIENT;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)) || defined(WITH_BACKPORTS)
         else if (session_type == WLAN_HDD_P2P_DEVICE)
            pAdapter->wdev.iftype = NL80211_IFTYPE_P2P_DEVICE;
#endif
         else
            pAdapter->wdev.iftype = NL80211_IFTYPE_STATION;

         pAdapter->device_mode = session_type;

         status = hdd_init_station_mode( pAdapter );
         if( VOS_STATUS_SUCCESS != status )
            goto err_free_netdev;

         // Workqueue which gets scheduled in IPv4 notification callback
         vos_init_work(&pAdapter->ipv4NotifierWorkQueue,
                        hdd_ipv4_notifier_work_queue);

#ifdef WLAN_NS_OFFLOAD
         // Workqueue which gets scheduled in IPv6 notification callback.
         vos_init_work(&pAdapter->ipv6NotifierWorkQueue,
                        hdd_ipv6_notifier_work_queue);
#endif
         status = hdd_register_interface(pAdapter, rtnl_held);
         if (VOS_STATUS_SUCCESS != status) {
            hdd_deinit_adapter(pHddCtx, pAdapter, rtnl_held);
            goto err_free_netdev;
         }

         //Stop the Interface TX queue.
         hddLog(LOG1, FL("Disabling queues"));
         netif_tx_disable(pAdapter->dev);
         //netif_tx_disable(pWlanDev);
         netif_carrier_off(pAdapter->dev);

#ifdef QCA_LL_TX_FLOW_CT
         /* SAT mode default TX Flow control instance
          * This instance will be used for
          * STA mode, IBSS mode and TDLS mode */
         if (pAdapter->tx_flow_timer_initialized == VOS_FALSE) {
            vos_timer_init(&pAdapter->tx_flow_control_timer,
                           VOS_TIMER_TYPE_SW,
                           hdd_tx_resume_timer_expired_handler,
                           pAdapter);
            pAdapter->tx_flow_timer_initialized = VOS_TRUE;
         }
         WLANTL_RegisterTXFlowControl(pHddCtx->pvosContext,
                     hdd_tx_resume_cb,
                     pAdapter->sessionId,
                     (void *)pAdapter);
#endif /* QCA_LL_TX_FLOW_CT */

         break;
      }

      case WLAN_HDD_P2P_GO:
      case WLAN_HDD_SOFTAP:
      {
         pAdapter = hdd_wlan_create_ap_dev( pHddCtx, macAddr, (tANI_U8 *)iface_name );
         if( NULL == pAdapter )
         {
            hddLog(VOS_TRACE_LEVEL_FATAL,
                   FL("failed to allocate adapter for session %d"), session_type);
            return NULL;
         }

         pAdapter->wdev.iftype = (session_type == WLAN_HDD_SOFTAP) ?
                                  NL80211_IFTYPE_AP:
                                  NL80211_IFTYPE_P2P_GO;
         pAdapter->device_mode = session_type;

         status = hdd_init_ap_mode(pAdapter);
         if( VOS_STATUS_SUCCESS != status )
            goto err_free_netdev;

         status = hdd_register_hostapd( pAdapter, rtnl_held );
         if( VOS_STATUS_SUCCESS != status )
         {
            hdd_deinit_adapter(pHddCtx, pAdapter, rtnl_held);
            goto err_free_netdev;
         }

         hddLog(LOG1, FL("Disabling queues"));
         netif_tx_disable(pAdapter->dev);
         netif_carrier_off(pAdapter->dev);

         hdd_set_conparam( 1 );

         break;
      }
      case WLAN_HDD_MONITOR:
      {
         pAdapter = hdd_alloc_station_adapter( pHddCtx, macAddr, iface_name );
         if( NULL == pAdapter )
         {
            hddLog(VOS_TRACE_LEVEL_FATAL,
                   FL("failed to allocate adapter for session %d"), session_type);
            return NULL;
         }

         pAdapter->wdev.iftype = NL80211_IFTYPE_MONITOR;
         pAdapter->device_mode = session_type;
         status = hdd_register_interface( pAdapter, rtnl_held );
         pAdapter->dev->netdev_ops = &wlan_mon_drv_ops;
         hdd_init_tx_rx( pAdapter );
         set_bit(INIT_TX_RX_SUCCESS, &pAdapter->event_flags);
         //Set adapter to be used for data tx. It will use either GO or softap.
         pAdapter->sessionCtx.monitor.pAdapterForTx =
                           hdd_get_adapter(pAdapter->pHddCtx, WLAN_HDD_SOFTAP);
         if (NULL == pAdapter->sessionCtx.monitor.pAdapterForTx)
         {
            pAdapter->sessionCtx.monitor.pAdapterForTx =
                           hdd_get_adapter(pAdapter->pHddCtx, WLAN_HDD_P2P_GO);
         }
         /* This work queue will be used to transmit management packet over
          * monitor interface. */
         if (NULL == pAdapter->sessionCtx.monitor.pAdapterForTx) {
             hddLog(VOS_TRACE_LEVEL_ERROR,"%s:Failed:hdd_get_adapter",__func__);
             return NULL;
         }

         vos_init_work(&pAdapter->sessionCtx.monitor.pAdapterForTx->
                        monTxWorkQueue, hdd_mon_tx_work_queue);
      }
         break;
      case WLAN_HDD_FTM:
      {
         pAdapter = hdd_alloc_station_adapter( pHddCtx, macAddr, iface_name );

         if( NULL == pAdapter )
         {
            hddLog(VOS_TRACE_LEVEL_FATAL,
                   FL("failed to allocate adapter for session %d"), session_type);
            return NULL;
         }

         /* Assign NL80211_IFTYPE_STATION as interface type to resolve Kernel Warning
          * message while loading driver in FTM mode. */
         pAdapter->wdev.iftype = NL80211_IFTYPE_STATION;
         pAdapter->device_mode = session_type;
         status = hdd_register_interface( pAdapter, rtnl_held );

         hdd_init_tx_rx( pAdapter );

         //Stop the Interface TX queue.
         hddLog(LOG1, FL("Disabling queues"));
         netif_tx_disable(pAdapter->dev);
         netif_carrier_off(pAdapter->dev);
      }
         break;
      default:
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s Invalid session type %d",
                __func__, session_type);
         VOS_ASSERT(0);
         return NULL;
      }
   }

    cfgState = WLAN_HDD_GET_CFG_STATE_PTR( pAdapter );
    mutex_init(&cfgState->remain_on_chan_ctx_lock);

   if( VOS_STATUS_SUCCESS == status )
   {
#ifdef WLAN_FEATURE_MBSSID
      hdd_mbssid_apply_def_cfg_ini(pAdapter);
#endif
      //Add it to the hdd's session list.
      pHddAdapterNode = vos_mem_malloc( sizeof( hdd_adapter_list_node_t ) );
      if( NULL == pHddAdapterNode )
      {
         status = VOS_STATUS_E_NOMEM;
      }
      else
      {
         pHddAdapterNode->pAdapter = pAdapter;
         status = hdd_add_adapter_back ( pHddCtx,
                                         pHddAdapterNode );
      }
   }

   if( VOS_STATUS_SUCCESS != status )
   {
      if( NULL != pAdapter )
      {
         hdd_cleanup_adapter( pHddCtx, pAdapter, rtnl_held );
         pAdapter = NULL;
      }
      if( NULL != pHddAdapterNode )
      {
         vos_mem_free( pHddAdapterNode );
      }

      goto resume_bmps;
   }

   if(VOS_STATUS_SUCCESS == status)
   {
      wlan_hdd_set_concurrency_mode(pHddCtx, session_type);

      //Initialize the WoWL service
      if(!hdd_init_wowl(pAdapter))
      {
          hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hdd_init_wowl failed",__func__);
          goto err_free_netdev;
      }

      /* Adapter successfully added. Increment the vdev count  */
      pHddCtx->current_intf_count++;

      hddLog(VOS_TRACE_LEVEL_DEBUG,"%s: current_intf_count=%d", __func__,
                                    pHddCtx->current_intf_count);
#ifdef FEATURE_WLAN_STA_AP_MODE_DFS_DISABLE
      if (vos_get_concurrency_mode() == VOS_STA_SAP) {
          hdd_adapter_t *ap_adapter;

          ap_adapter = hdd_get_adapter(pHddCtx, WLAN_HDD_SOFTAP);
          if (ap_adapter != NULL &&
              test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags) &&
              VOS_IS_DFS_CH(ap_adapter->sessionCtx.ap.operatingChannel)) {

              hddLog(VOS_TRACE_LEVEL_WARN,
                  "STA-AP Mode DFS not supported. Restart SAP with Non DFS ACS"
                  );
              ap_adapter->sessionCtx.ap.sapConfig.channel = AUTO_CHANNEL_SELECT;
              ap_adapter->sessionCtx.ap.sapConfig.acs_cfg.acs_mode = true;
              wlan_hdd_restart_sap(ap_adapter);
          }
      }
#endif
   }

   if ((vos_get_conparam() != VOS_FTM_MODE) && (!pHddCtx->cfg_ini->enable2x2))
   {
#define HDD_DTIM_1CHAIN_RX_ID 0x5
#define HDD_SMPS_PARAM_VALUE_S 29

      /* Disable DTIM 1 chain Rx when in 1x1, we are passing two values as
         param_id << 29 | param_value. Below param_value = 0(disable) */
      ret = process_wma_set_command((int)pAdapter->sessionId,
                             (int)WMI_STA_SMPS_PARAM_CMDID,
                             HDD_DTIM_1CHAIN_RX_ID << HDD_SMPS_PARAM_VALUE_S,
                             VDEV_CMD);

      if (ret != 0)
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,"%s: DTIM 1 chain set failed %d", __func__, ret);
         goto err_free_netdev;
      }

        if (!pHddCtx->per_band_chainmask_supp) {
            ret = process_wma_set_command((int)pAdapter->sessionId,
                        (int)WMI_PDEV_PARAM_TX_CHAIN_MASK,
                        (int)pHddCtx->cfg_ini->txchainmask1x1, PDEV_CMD);
            if (ret != 0)
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                                "%s: PDEV_PARAM_TX_CHAIN_MASK set failed %d",
                                __func__, ret);
                goto err_free_netdev;
            }
            ret = process_wma_set_command((int)pAdapter->sessionId,
                            (int)WMI_PDEV_PARAM_RX_CHAIN_MASK,
                            (int)pHddCtx->cfg_ini->rxchainmask1x1, PDEV_CMD);
            if (ret != 0)
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                                "%s: WMI_PDEV_PARAM_RX_CHAIN_MASK set failed %d",
                                __func__, ret);
                goto err_free_netdev;
            }
        }
#undef HDD_DTIM_1CHAIN_RX_ID
#undef HDD_SMPS_PARAM_VALUE_S
   }

  if (VOS_FTM_MODE != vos_get_conparam())
  {
       ret = process_wma_set_command((int)pAdapter->sessionId,
                         (int)WMI_PDEV_PARAM_HYST_EN,
                         (int)pHddCtx->cfg_ini->enableMemDeepSleep,
                         PDEV_CMD);

       if (ret != 0)
       {
           hddLog(VOS_TRACE_LEVEL_ERROR,"%s: WMI_PDEV_PARAM_HYST_EN set"
                                 " failed %d", __func__, ret);
           goto err_free_netdev;
       }

        hddLog(LOG1, FL("SET AMSDU num %d"), pHddCtx->cfg_ini->max_amsdu_num);

        ret = process_wma_set_command((int)pAdapter->sessionId,
                        (int)GEN_VDEV_PARAM_AMSDU,
                        (int)pHddCtx->cfg_ini->max_amsdu_num,
                        GEN_CMD);
        if (ret != 0) {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                        FL("GEN_VDEV_PARAM_AMSDU set failed %d"), ret);
                goto err_free_netdev;
        }
  }


#ifdef CONFIG_FW_LOGS_BASED_ON_INI

  /* Enable FW logs based on INI configuration */
  if ((VOS_FTM_MODE != vos_get_conparam()) &&
             (pHddCtx->cfg_ini->enablefwlog))
  {
     tANI_U8 count = 0;
     tANI_U32 value = 0;
     tANI_U8 numEntries = 0;
     tANI_U8 moduleLoglevel[FW_MODULE_LOG_LEVEL_STRING_LENGTH];

     pHddCtx->fw_log_settings.dl_type = pHddCtx->cfg_ini->enableFwLogType;
     ret = process_wma_set_command( (int)pAdapter->sessionId,
                                  (int)WMI_DBGLOG_TYPE,
                                  pHddCtx->cfg_ini->enableFwLogType, DBG_CMD );
     if (ret != 0)
     {
          hddLog(LOGE, FL("Failed to enable FW log type ret %d"), ret);
     }

     pHddCtx->fw_log_settings.dl_loglevel = pHddCtx->cfg_ini->enableFwLogLevel;
     ret = process_wma_set_command((int)pAdapter->sessionId,
                                   (int)WMI_DBGLOG_LOG_LEVEL,
                                   pHddCtx->cfg_ini->enableFwLogLevel, DBG_CMD);
     if (ret != 0)
     {
          hddLog(LOGE, FL("Failed to enable FW log level ret %d"), ret);
     }

     hdd_string_to_u8_array( pHddCtx->cfg_ini->enableFwModuleLogLevel,
                             moduleLoglevel,
                             &numEntries,
                             FW_MODULE_LOG_LEVEL_STRING_LENGTH);

     while (count < numEntries)
     {
         /* FW module log level input string looks like below:
            gFwDebugModuleLoglevel=<FW Module ID>, <Log Level>, so on....
            For example:
            gFwDebugModuleLoglevel=1,0,2,1,3,2,4,3,5,4,6,5,7,6,8,7
            Above input string means :
            For FW module ID 1 enable log level 0
            For FW module ID 2 enable log level 1
            For FW module ID 3 enable log level 2
            For FW module ID 4 enable log level 3
            For FW module ID 5 enable log level 4
            For FW module ID 6 enable log level 5
            For FW module ID 7 enable log level 6
            For FW module ID 8 enable log level 7
         */
         /* FW expects WMI command value = Module ID * 10 + Module Log level */
         value = ( (moduleLoglevel[count] * 10) + moduleLoglevel[count + 1] );
         ret = process_wma_set_command((int)pAdapter->sessionId,
                                       (int)WMI_DBGLOG_MOD_LOG_LEVEL,
                                       value, DBG_CMD);
         if (ret != 0)
         {
            hddLog(LOGE, FL("Failed to enable FW module log level %d ret %d"),
              value, ret);
         }

         count += 2;
     }
  }

#endif

     ret = process_wma_set_command((int)pAdapter->sessionId,
                                (int)WMI_VDEV_PARAM_ENABLE_RTSCTS,
                                pHddCtx->cfg_ini->rts_profile, VDEV_CMD);
     if (ret != 0)
        hddLog(LOGE, "FAILED TO SET RTSCTS Profile ret:%d", ret);


   return pAdapter;

err_free_netdev:
   free_netdev(pAdapter->dev);
   wlan_hdd_release_intf_addr( pHddCtx,
                               pAdapter->macAddressCurrent.bytes );

resume_bmps:
   //If bmps disabled enable it
   if (!pHddCtx->cfg_ini->enablePowersaveOffload)
   {
       if(VOS_STATUS_SUCCESS == exitbmpsStatus)
       {
          if (pHddCtx->hdd_wlan_suspended)
          {
             hdd_set_pwrparams(pHddCtx);
          }
          hdd_enable_bmps_imps(pHddCtx);
       }
   }

   return NULL;
}

VOS_STATUS hdd_close_adapter( hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter,
                              tANI_U8 rtnl_held )
{
   hdd_adapter_list_node_t *pAdapterNode, *pCurrent, *pNext;
   VOS_STATUS status;

   status = hdd_get_front_adapter ( pHddCtx, &pCurrent );
   if( VOS_STATUS_SUCCESS != status )
   {
      hddLog(VOS_TRACE_LEVEL_WARN,"%s: adapter list empty %d",
             __func__, status);
      return status;
   }

   while ( pCurrent->pAdapter != pAdapter )
   {
      status = hdd_get_next_adapter ( pHddCtx, pCurrent, &pNext );
      if( VOS_STATUS_SUCCESS != status )
         break;

      pCurrent = pNext;
   }
   pAdapterNode = pCurrent;
   if( VOS_STATUS_SUCCESS == status )
   {
      wlan_hdd_clear_concurrency_mode(pHddCtx, pAdapter->device_mode);
      hdd_cleanup_adapter( pHddCtx, pAdapterNode->pAdapter, rtnl_held );

      hdd_remove_adapter( pHddCtx, pAdapterNode );
      vos_mem_free( pAdapterNode );
      pAdapterNode = NULL;

      /* Adapter removed. Decrement vdev count */
      if (pHddCtx->current_intf_count != 0)
        pHddCtx->current_intf_count--;

      /*
       * If Powersave Offload is enabled,
       * Fw will take care incase of concurrency
       */
      if(pHddCtx->cfg_ini->enablePowersaveOffload)
          return VOS_STATUS_SUCCESS;

      /* If there is a single session of STA/P2P client, re-enable BMPS */
      if ((!vos_concurrent_open_sessions_running()) &&
           ((pHddCtx->no_of_open_sessions[VOS_STA_MODE] >= 1) ||
           (pHddCtx->no_of_open_sessions[VOS_P2P_CLIENT_MODE] >= 1)))
      {
          if (pHddCtx->hdd_wlan_suspended)
          {
              hdd_set_pwrparams(pHddCtx);
          }
          hdd_enable_bmps_imps(pHddCtx);
      }

      return VOS_STATUS_SUCCESS;
   }

   return VOS_STATUS_E_FAILURE;
}

VOS_STATUS hdd_close_all_adapters( hdd_context_t *pHddCtx )
{
   hdd_adapter_list_node_t *pHddAdapterNode;
   VOS_STATUS status;

   ENTER();

   do
   {
      status = hdd_remove_front_adapter( pHddCtx, &pHddAdapterNode );
      if( pHddAdapterNode && VOS_STATUS_SUCCESS == status )
      {
         hdd_cleanup_adapter( pHddCtx, pHddAdapterNode->pAdapter, FALSE );
         vos_mem_free( pHddAdapterNode );
      }
   }while( NULL != pHddAdapterNode && VOS_STATUS_E_EMPTY != status );

   EXIT();

   return VOS_STATUS_SUCCESS;
}

void wlan_hdd_reset_prob_rspies(hdd_adapter_t* pHostapdAdapter)
{
    tANI_U8 *bssid = NULL;
    tSirUpdateIE updateIE;
    switch (pHostapdAdapter->device_mode)
    {
    case WLAN_HDD_INFRA_STATION:
    case WLAN_HDD_P2P_CLIENT:
    {
        hdd_station_ctx_t * pHddStaCtx =
            WLAN_HDD_GET_STATION_CTX_PTR(pHostapdAdapter);
        bssid = (tANI_U8*)&pHddStaCtx->conn_info.bssId;
        break;
    }
    case WLAN_HDD_SOFTAP:
    case WLAN_HDD_P2P_GO:
    case WLAN_HDD_IBSS:
    {
        bssid = pHostapdAdapter->macAddressCurrent.bytes;
        break;
    }
    case WLAN_HDD_MONITOR:
    case WLAN_HDD_FTM:
    case WLAN_HDD_P2P_DEVICE:
    default:
        /*
         * wlan_hdd_reset_prob_rspies should not have been called
         * for these kind of devices
         */
        hddLog(LOGE, FL("Unexpected request for the current device type %d"),
               pHostapdAdapter->device_mode);
        return;
    }

    vos_mem_copy(updateIE.bssid, bssid, sizeof(tSirMacAddr));
        updateIE.smeSessionId =  pHostapdAdapter->sessionId;
        updateIE.ieBufferlength = 0;
        updateIE.pAdditionIEBuffer = NULL;
        updateIE.append = VOS_TRUE;
        updateIE.notify = VOS_FALSE;
    if (sme_UpdateAddIE(WLAN_HDD_GET_HAL_CTX(pHostapdAdapter),
            &updateIE, eUPDATE_IE_PROBE_RESP) == eHAL_STATUS_FAILURE) {
        hddLog(LOGE, FL("Could not pass on PROBE_RSP_BCN data to PE"));
    }
}

VOS_STATUS hdd_stop_adapter( hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter,
                             const v_BOOL_t bCloseSession)
{
   eHalStatus halStatus = eHAL_STATUS_SUCCESS;
   hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
   union iwreq_data wrqu;
   tSirUpdateIE updateIE ;
   unsigned long rc;

   ENTER();

   hddLog(LOG1, FL("Disabling queues"));
   netif_tx_disable(pAdapter->dev);
   netif_carrier_off(pAdapter->dev);
   switch(pAdapter->device_mode)
   {
      case WLAN_HDD_INFRA_STATION:
      case WLAN_HDD_IBSS:
      case WLAN_HDD_P2P_CLIENT:
      case WLAN_HDD_P2P_DEVICE:
         if (hdd_connIsConnected(WLAN_HDD_GET_STATION_CTX_PTR(pAdapter)) ||
            hdd_is_connecting(WLAN_HDD_GET_STATION_CTX_PTR(pAdapter)))
         {
            if (pWextState->roamProfile.BSSType == eCSR_BSS_TYPE_START_IBSS)
                halStatus = sme_RoamDisconnect(pHddCtx->hHal,
                                             pAdapter->sessionId,
                                             eCSR_DISCONNECT_REASON_IBSS_LEAVE);
            else
                halStatus = sme_RoamDisconnect(pHddCtx->hHal,
                                            pAdapter->sessionId,
                                            eCSR_DISCONNECT_REASON_UNSPECIFIED);
            //success implies disconnect command got queued up successfully
            if(halStatus == eHAL_STATUS_SUCCESS)
            {
               rc = wait_for_completion_timeout(
                          &pAdapter->disconnect_comp_var,
                          msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
               if (!rc) {
                  hddLog(VOS_TRACE_LEVEL_ERROR,
                         "%s: wait on disconnect_comp_var failed",
                         __func__);
               }
           }
           else
           {
               hddLog(LOGE, "%s: failed to post disconnect event to SME",
                      __func__);
            }
            memset(&wrqu, '\0', sizeof(wrqu));
            wrqu.ap_addr.sa_family = ARPHRD_ETHER;
            memset(wrqu.ap_addr.sa_data,'\0',ETH_ALEN);
            wireless_send_event(pAdapter->dev, SIOCGIWAP, &wrqu, NULL);
         }
         else
         {
            hdd_abort_mac_scan(pHddCtx, pAdapter->sessionId,
                               eCSR_SCAN_ABORT_DEFAULT);
         }
         if ((pAdapter->device_mode == WLAN_HDD_P2P_CLIENT) ||
              (pAdapter->device_mode == WLAN_HDD_P2P_DEVICE)) {
             wlan_hdd_cleanup_remain_on_channel_ctx(pAdapter);
         }

#ifdef WLAN_OPEN_SOURCE
         cancel_work_sync(&pAdapter->ipv4NotifierWorkQueue);
#endif

         wlan_hdd_clean_tx_flow_control_timer(pHddCtx, pAdapter);

#ifdef WLAN_NS_OFFLOAD
#ifdef WLAN_OPEN_SOURCE
         cancel_work_sync(&pAdapter->ipv6NotifierWorkQueue);
#endif
#endif

         /* It is possible that the caller of this function does not
          * wish to close the session
          */
         if (VOS_TRUE == bCloseSession &&
             test_bit(SME_SESSION_OPENED, &pAdapter->event_flags))
         {
            INIT_COMPLETION(pAdapter->session_close_comp_var);
            if (eHAL_STATUS_SUCCESS ==
                  sme_CloseSession(pHddCtx->hHal, pAdapter->sessionId,
                     hdd_smeCloseSessionCallback, pAdapter))
            {
               //Block on a completion variable. Can't wait forever though.
               rc = wait_for_completion_timeout(
                     &pAdapter->session_close_comp_var,
                     msecs_to_jiffies(WLAN_WAIT_TIME_SESSIONOPENCLOSE));
               if (!rc) {
                  hddLog(LOGE, "%s: failure waiting for session_close_comp_var",
                        __func__);
               }
            }
         }
         break;

      case WLAN_HDD_SOFTAP:
      case WLAN_HDD_P2P_GO:
         if (pHddCtx->cfg_ini->conc_custom_rule1 &&
             (WLAN_HDD_SOFTAP == pAdapter->device_mode)) {
             /*
              * Before stopping the sap adapter, lets make sure there is
              * no sap restart work pending.
              */
             vos_flush_work(&pHddCtx->sap_start_work);
             VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
                        FL("Canceled the pending SAP restart work"));
             hdd_change_sap_restart_required_status(pHddCtx, false);
         }
         //Any softap specific cleanup here...
         if (pAdapter->device_mode == WLAN_HDD_P2P_GO) {
             wlan_hdd_cleanup_remain_on_channel_ctx(pAdapter);
         }

         hdd_set_sap_auth_offload(pAdapter, FALSE);

         wlan_hdd_clean_tx_flow_control_timer(pHddCtx, pAdapter);

         mutex_lock(&pHddCtx->sap_lock);
         if (test_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags))
         {
            VOS_STATUS status;
            hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
            hdd_hostapd_state_t *pHostapdState =
                  WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);
            vos_event_reset(&pHostapdState->stop_bss_event);

            //Stop Bss.
#ifdef WLAN_FEATURE_MBSSID
            status = WLANSAP_StopBss(WLAN_HDD_GET_SAP_CTX_PTR(pAdapter));
#else
            status = WLANSAP_StopBss(pHddCtx->pvosContext);
#endif

            if (VOS_IS_STATUS_SUCCESS(status))
            {
               status = vos_wait_single_event(&pHostapdState->stop_bss_event,
                                              10000);
               if (!VOS_IS_STATUS_SUCCESS(status))
               {
                  hddLog(LOGE, "%s: failure waiting for WLANSAP_StopBss %d",
                         __func__, status);
               }
            }
            else
            {
               hddLog(LOGE, "%s: failure in WLANSAP_StopBss", __func__);
            }
            clear_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags);
            wlan_hdd_decr_active_session(pHddCtx, pAdapter->device_mode);

            vos_mem_copy(updateIE.bssid, pAdapter->macAddressCurrent.bytes,
                   sizeof(tSirMacAddr));
            updateIE.smeSessionId = pAdapter->sessionId;
            updateIE.ieBufferlength = 0;
            updateIE.pAdditionIEBuffer = NULL;
            updateIE.append = VOS_FALSE;
            updateIE.notify = VOS_FALSE;
            /* Probe bcn reset */
            if (sme_UpdateAddIE(WLAN_HDD_GET_HAL_CTX(pAdapter),
                              &updateIE, eUPDATE_IE_PROBE_BCN)
                              == eHAL_STATUS_FAILURE) {
                hddLog(LOGE, FL("Could not pass on PROBE_RSP_BCN data to PE"));
            }
            /* Assoc resp reset */
            if (sme_UpdateAddIE(WLAN_HDD_GET_HAL_CTX(pAdapter),
                    &updateIE, eUPDATE_IE_ASSOC_RESP) == eHAL_STATUS_FAILURE) {
                hddLog(LOGE, FL("Could not pass on ASSOC_RSP data to PE"));
            }

            // Reset WNI_CFG_PROBE_RSP Flags
            wlan_hdd_reset_prob_rspies(pAdapter);
            kfree(pAdapter->sessionCtx.ap.beacon);
            pAdapter->sessionCtx.ap.beacon = NULL;
         }
         mutex_unlock(&pHddCtx->sap_lock);
         break;

      case WLAN_HDD_MONITOR:
#ifdef WLAN_OPEN_SOURCE
         cancel_work_sync(&pAdapter->sessionCtx.monitor.pAdapterForTx->monTxWorkQueue);
#endif
         break;

      case WLAN_HDD_OCB:
         hdd_disconnect_tx_rx(pAdapter);
         WLANTL_ClearSTAClient(WLAN_HDD_GET_CTX(pAdapter)->pvosContext,
            WLAN_HDD_GET_STATION_CTX_PTR(pAdapter)->conn_info.staId[0]);
         break;

      default:
         break;
   }

   EXIT();
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_stop_all_adapters( hdd_context_t *pHddCtx )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   hdd_adapter_t      *pAdapter;

   ENTER();

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;
      hdd_stop_adapter( pHddCtx, pAdapter, VOS_TRUE );
      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   EXIT();

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_reset_all_adapters( hdd_context_t *pHddCtx )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   hdd_adapter_t *pAdapter;

   ENTER();

   status =  hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;
      hddLog(LOG1, FL("Disabling queues"));
      netif_tx_disable(pAdapter->dev);
      netif_carrier_off(pAdapter->dev);

      pAdapter->sessionCtx.station.hdd_ReassocScenario = VOS_FALSE;

      hdd_deinit_tx_rx(pAdapter);
      wlan_hdd_decr_active_session(pHddCtx, pAdapter->device_mode);
      if (test_bit(WMM_INIT_DONE, &pAdapter->event_flags))
      {
          hdd_wmm_adapter_close( pAdapter );
          clear_bit(WMM_INIT_DONE, &pAdapter->event_flags);
      }

      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   EXIT();

   return VOS_STATUS_SUCCESS;
}

/**
 * hdd_connect_result() - API to send connection status to supplicant
 * @dev: network device
 * @bssid: bssid to which we want to associate
 * @req_ie: Request Information Element
 * @req_ie_len: len of the req IE
 * @resp_ie: Response IE
 * @resp_ie_len: len of ht response IE
 * @status: status
 * @gfp: Kernel Flag
 *
 * The API is a wrapper to send connection status to supplicant
 * and allow runtime suspend
 *
 * Return: Void
 */

void hdd_connect_result(struct net_device *dev,
			const u8 *bssid,
			const u8 *req_ie,
			size_t req_ie_len,
			const u8 * resp_ie,
			size_t resp_ie_len,
			u16 status,
			gfp_t gfp)
{
	hdd_adapter_t *padapter = (hdd_adapter_t *) netdev_priv(dev);

	cfg80211_connect_result(dev, bssid, req_ie, req_ie_len,
				resp_ie, resp_ie_len, status, gfp);

	vos_runtime_pm_allow_suspend(padapter->runtime_context.connect);
}


VOS_STATUS hdd_start_all_adapters( hdd_context_t *pHddCtx )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   hdd_adapter_t      *pAdapter;
#if !defined(MSM_PLATFORM) || defined(WITH_BACKPORTS)
   v_MACADDR_t  bcastMac = VOS_MAC_ADDR_BROADCAST_INITIALIZER;
#endif
   eConnectionState  connState;

   ENTER();

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;

      hdd_wmm_init( pAdapter );

      switch(pAdapter->device_mode)
      {
         case WLAN_HDD_INFRA_STATION:
         case WLAN_HDD_P2P_CLIENT:
         case WLAN_HDD_P2P_DEVICE:

            connState = (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState;

            hdd_init_station_mode(pAdapter);
            /* Open the gates for HDD to receive Wext commands */
            pAdapter->isLinkUpSvcNeeded = FALSE;
            pAdapter->scan_info.mScanPending = FALSE;
            pAdapter->scan_info.waitScanResult = FALSE;

            //Indicate disconnect event to supplicant if associated previously
            if (eConnectionState_Associated == connState ||
                eConnectionState_IbssConnected == connState ||
                eConnectionState_NotConnected == connState ||
                eConnectionState_IbssDisconnected == connState ||
                eConnectionState_Disconnecting == connState)
            {
               union iwreq_data wrqu;
               memset(&wrqu, '\0', sizeof(wrqu));
               wrqu.ap_addr.sa_family = ARPHRD_ETHER;
               memset(wrqu.ap_addr.sa_data,'\0',ETH_ALEN);
               wireless_send_event(pAdapter->dev, SIOCGIWAP, &wrqu, NULL);
               pAdapter->sessionCtx.station.hdd_ReassocScenario = VOS_FALSE;

               /* indicate disconnected event to nl80211 */
               cfg80211_disconnected(pAdapter->dev, WLAN_REASON_UNSPECIFIED,
                                     NULL, 0, GFP_KERNEL);
            }
            else if (eConnectionState_Connecting == connState)
            {
              /*
               * Indicate connect failure to supplicant if we were in the
               * process of connecting
               */
               hdd_connect_result(pAdapter->dev, NULL,
                                       NULL, 0, NULL, 0,
                                       WLAN_STATUS_ASSOC_DENIED_UNSPEC,
                                       GFP_KERNEL);
            }

#ifdef QCA_LL_TX_FLOW_CT
            WLANTL_RegisterTXFlowControl(pHddCtx->pvosContext, hdd_tx_resume_cb,
                                         pAdapter->sessionId, (void *)pAdapter);
#endif

            break;

         case WLAN_HDD_SOFTAP:
            /* softAP can handle SSR */
            break;

         case WLAN_HDD_P2P_GO:
#if defined(MSM_PLATFORM) && !defined(WITH_BACKPORTS)
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s [SSR] send stop ap to supplicant",
                                                       __func__);
            cfg80211_ap_stopped(pAdapter->dev, GFP_KERNEL);
#else
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s [SSR] send restart supplicant",
                                                       __func__);
            /* event supplicant to restart */
            cfg80211_del_sta(pAdapter->dev,
                      (const u8 *)&bcastMac.bytes[0], GFP_KERNEL);
#endif
            break;

         case WLAN_HDD_MONITOR:
            /* monitor interface start */
            break;
         default:
            break;
      }

      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   EXIT();

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_reconnect_all_adapters( hdd_context_t *pHddCtx )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   hdd_adapter_t *pAdapter;
   VOS_STATUS status;
   v_U32_t roamId;
   unsigned long rc;

   ENTER();

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;

      if( (WLAN_HDD_INFRA_STATION == pAdapter->device_mode) ||
             (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode) )
      {
         hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
         hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

         hddLog(VOS_TRACE_LEVEL_INFO,
            "%s: Set HDD connState to eConnectionState_NotConnected",
                   __func__);
         if (eConnectionState_Associated == pHddStaCtx->conn_info.connState) {
             wlan_hdd_decr_active_session(pHddCtx, pAdapter->device_mode);
         }
         pHddStaCtx->conn_info.connState = eConnectionState_NotConnected;
         init_completion(&pAdapter->disconnect_comp_var);
         sme_RoamDisconnect(pHddCtx->hHal, pAdapter->sessionId,
                             eCSR_DISCONNECT_REASON_UNSPECIFIED);

         rc = wait_for_completion_timeout(
                        &pAdapter->disconnect_comp_var,
                        msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
         if (!rc)
            hddLog(LOGE, "%s: failure waiting for disconnect_comp_var",
                   __func__);
         pWextState->roamProfile.csrPersona = pAdapter->device_mode;
         pHddCtx->isAmpAllowed = VOS_FALSE;
         sme_RoamConnect(pHddCtx->hHal,
                         pAdapter->sessionId, &(pWextState->roamProfile),
                         &roamId);
      }

      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   EXIT();

   return VOS_STATUS_SUCCESS;
}

void hdd_dump_concurrency_info(hdd_context_t *pHddCtx)
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   hdd_adapter_t *pAdapter;
   hdd_station_ctx_t *pHddStaCtx;
   hdd_ap_ctx_t *pHddApCtx;
   hdd_hostapd_state_t * pHostapdState;
   tCsrBssid staBssid = { 0 }, p2pBssid = { 0 };
   tCsrBssid apBssid = { 0 }, apBssid1 = { 0 }, apBssid2 = { 0 };
   v_U8_t staChannel = 0, p2pChannel = 0, apChannel = 0;
   v_U8_t apChannel1 = 0, apChannel2 = 0;
   const char *p2pMode = "DEV";
   bool mcc_mode = FALSE;

#ifdef QCA_LL_TX_FLOW_CT
   v_U8_t targetChannel = 0;
   v_U8_t preAdapterChannel = 0;
   v_U8_t channel24;
   v_U8_t channel5;
   hdd_adapter_t *preAdapterContext = NULL;
   hdd_adapter_t *pAdapter2_4 = NULL;
   hdd_adapter_t *pAdapter5 = NULL;
#endif /* QCA_LL_TX_FLOW_CT */

   status =  hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;
      switch (pAdapter->device_mode) {
      case WLAN_HDD_INFRA_STATION:
          pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
          if (eConnectionState_Associated == pHddStaCtx->conn_info.connState) {
              staChannel = pHddStaCtx->conn_info.operationChannel;
              memcpy(staBssid, pHddStaCtx->conn_info.bssId, sizeof(staBssid));
#ifdef QCA_LL_TX_FLOW_CT
              targetChannel = staChannel;
#endif /* QCA_LL_TX_FLOW_CT */
          }
          break;
      case WLAN_HDD_P2P_CLIENT:
          pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
          if (eConnectionState_Associated == pHddStaCtx->conn_info.connState) {
              p2pChannel = pHddStaCtx->conn_info.operationChannel;
              memcpy(p2pBssid, pHddStaCtx->conn_info.bssId, sizeof(p2pBssid));
              p2pMode = "CLI";
#ifdef QCA_LL_TX_FLOW_CT
              targetChannel = p2pChannel;
#endif /* QCA_LL_TX_FLOW_CT */
          }
          break;
      case WLAN_HDD_P2P_GO:
          pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);
          pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);
          if (pHostapdState->bssState == BSS_START && pHostapdState->vosStatus==VOS_STATUS_SUCCESS) {
              p2pChannel = pHddApCtx->operatingChannel;
              memcpy(p2pBssid, pAdapter->macAddressCurrent.bytes, sizeof(p2pBssid));
#ifdef QCA_LL_TX_FLOW_CT
              targetChannel = p2pChannel;
#endif /* QCA_LL_TX_FLOW_CT */
          }
          p2pMode = "GO";
          break;
      case WLAN_HDD_SOFTAP:
          pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);
          pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);
          if (pHostapdState->bssState == BSS_START && pHostapdState->vosStatus==VOS_STATUS_SUCCESS) {
              apChannel = pHddApCtx->operatingChannel;
              memcpy(apBssid, pAdapter->macAddressCurrent.bytes, sizeof(apBssid));
              if (!pHddApCtx->uBCStaId) {
                  apChannel1 = apChannel;
                  memcpy(apBssid1, apBssid, sizeof(apBssid));
              } else {
                  apChannel2 = apChannel;
                  memcpy(apBssid2, apBssid, sizeof(apBssid));
              }
#ifdef QCA_LL_TX_FLOW_CT
              targetChannel = apChannel;
#endif /* QCA_LL_TX_FLOW_CT */
          }
          break;
      case WLAN_HDD_IBSS:
          return; /* skip printing station message below */
      default:
          break;
      }
#ifdef QCA_LL_TX_FLOW_CT
      if (targetChannel)
      {
         /* This is first adapter detected as active
          * set as default for none concurrency case */
         if (!preAdapterChannel)
         {
#ifdef IPA_UC_OFFLOAD
             /* If IPA UC data path is enabled,
              * target should reserve extra tx descriptors
              * for IPA WDI data path.
              * Then host data path should allow less TX packet pumping in case
              * IPA WDI data path enabled */
             if ((pHddCtx->cfg_ini->IpaUcOffloadEnabled) &&
                 (WLAN_HDD_SOFTAP == pAdapter->device_mode)) {
                pAdapter->tx_flow_low_watermark =
                       pHddCtx->cfg_ini->TxFlowLowWaterMark +
                       WLAN_TFC_IPAUC_TX_DESC_RESERVE;
             } else
#endif /* IPA_UC_OFFLOAD */
#ifdef CONFIG_HL_SUPPORT
             {
                 pAdapter->tx_flow_low_watermark = 0;
             }
             pAdapter->tx_flow_high_watermark_offset = 0;
#else
             {
                 pAdapter->tx_flow_low_watermark =
                       pHddCtx->cfg_ini->TxFlowLowWaterMark;
             }
             pAdapter->tx_flow_high_watermark_offset =
                       pHddCtx->cfg_ini->TxFlowHighWaterMarkOffset;
#endif
             WLANTL_SetAdapterMaxQDepth(pHddCtx->pvosContext,
                                        pAdapter->sessionId,
                                        pHddCtx->cfg_ini->TxFlowMaxQueueDepth);
             /* Temporary set log level as error
              * TX Flow control feature settled down, will lower log level */
             hddLog(LOG1,
                    "MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
                    hdd_device_mode_to_string(pAdapter->device_mode),
                    pAdapter->device_mode,
                    targetChannel,
                    pAdapter->tx_flow_low_watermark,
                    pAdapter->tx_flow_low_watermark +
                    pAdapter->tx_flow_high_watermark_offset,
                    pHddCtx->cfg_ini->TxFlowMaxQueueDepth);
             preAdapterChannel = targetChannel;
             preAdapterContext = pAdapter;
         }
         else
         {
            /* SCC, disable TX flow control for both
             * SCC each adapter cannot reserve dedicated channel resource
             * as a result, if any adapter blocked OS Q by flow control,
             * blocked adapter will lost chance to recover  */
            if (preAdapterChannel == targetChannel)
            {
                /* Current adapter */
                pAdapter->tx_flow_low_watermark = 0;
                pAdapter->tx_flow_high_watermark_offset = 0;
                WLANTL_SetAdapterMaxQDepth(pHddCtx->pvosContext,
                                           pAdapter->sessionId,
                                           pHddCtx->cfg_ini->TxHbwFlowMaxQueueDepth);
                hddLog(LOG1,
                      "SCC: MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
                      hdd_device_mode_to_string(pAdapter->device_mode),
                      pAdapter->device_mode,
                      targetChannel,
                      pAdapter->tx_flow_low_watermark,
                      pAdapter->tx_flow_low_watermark +
                      pAdapter->tx_flow_high_watermark_offset,
                      pHddCtx->cfg_ini->TxHbwFlowMaxQueueDepth);

                if (!preAdapterContext)
                {
                   hddLog(VOS_TRACE_LEVEL_ERROR,
                      "SCC: Previous adapter context NULL");
                   continue;
                }

                /* Previous adapter */
                preAdapterContext->tx_flow_low_watermark = 0;
                preAdapterContext->tx_flow_high_watermark_offset = 0;
                WLANTL_SetAdapterMaxQDepth(pHddCtx->pvosContext,
                                           preAdapterContext->sessionId,
                                           pHddCtx->cfg_ini->TxHbwFlowMaxQueueDepth);
                /* Temporary set log level as error
                 * TX Flow control feature settled down, will lower log level */
                hddLog(LOG1,
                      "SCC: MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
                      hdd_device_mode_to_string(preAdapterContext->device_mode),
                      preAdapterContext->device_mode,
                      targetChannel,
                      preAdapterContext->tx_flow_low_watermark,
                      preAdapterContext->tx_flow_low_watermark +
                      preAdapterContext->tx_flow_high_watermark_offset,
                      pHddCtx->cfg_ini->TxHbwFlowMaxQueueDepth);
            }
            /* MCC, each adapter will have dedicated resource */
            else
            {
                /* current channel is 2.4 */
                if (targetChannel <= WLAN_HDD_TX_FLOW_CONTROL_MAX_24BAND_CH)
                {
                   channel24   = targetChannel;
                   channel5    = preAdapterChannel;
                   pAdapter2_4 = pAdapter;
                   pAdapter5   = preAdapterContext;
                }
                /* Current channel is 5 */
                else
                {
                   channel24   = preAdapterChannel;
                   channel5    = targetChannel;
                   pAdapter2_4 = preAdapterContext;
                   pAdapter5   = pAdapter;
                }

                if (!pAdapter5)
                {
                   hddLog(VOS_TRACE_LEVEL_ERROR,
                      "MCC: 5GHz adapter context NULL");
                   continue;
                }
                pAdapter5->tx_flow_low_watermark =
                       pHddCtx->cfg_ini->TxHbwFlowLowWaterMark;
                pAdapter5->tx_flow_high_watermark_offset =
                       pHddCtx->cfg_ini->TxHbwFlowHighWaterMarkOffset;
                WLANTL_SetAdapterMaxQDepth(pHddCtx->pvosContext,
                                        pAdapter5->sessionId,
                                        pHddCtx->cfg_ini->TxHbwFlowMaxQueueDepth);
                /* Temporary set log level as error
                 * TX Flow control feature settled down, will lower log level */
                hddLog(LOG1,
                    "MCC: MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
                    hdd_device_mode_to_string(pAdapter5->device_mode),
                    pAdapter5->device_mode,
                    channel5,
                    pAdapter5->tx_flow_low_watermark,
                    pAdapter5->tx_flow_low_watermark +
                    pAdapter5->tx_flow_high_watermark_offset,
                    pHddCtx->cfg_ini->TxHbwFlowMaxQueueDepth);

                if (!pAdapter2_4)
                {
                   hddLog(VOS_TRACE_LEVEL_ERROR,
                      "MCC: 2.4GHz adapter context NULL");
                   continue;
                }
                pAdapter2_4->tx_flow_low_watermark =
                       pHddCtx->cfg_ini->TxLbwFlowLowWaterMark;
                pAdapter2_4->tx_flow_high_watermark_offset =
                       pHddCtx->cfg_ini->TxLbwFlowHighWaterMarkOffset;
                WLANTL_SetAdapterMaxQDepth(pHddCtx->pvosContext,
                                        pAdapter2_4->sessionId,
                                        pHddCtx->cfg_ini->TxLbwFlowMaxQueueDepth);
                /* Temporary set log level as error
                 * TX Flow control feature settled down, will lower log level */
                hddLog(LOG1,
                    "MCC: MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
                    hdd_device_mode_to_string(pAdapter2_4->device_mode),
                    pAdapter2_4->device_mode,
                    channel24,
                    pAdapter2_4->tx_flow_low_watermark,
                    pAdapter2_4->tx_flow_low_watermark +
                    pAdapter2_4->tx_flow_high_watermark_offset,
                    pHddCtx->cfg_ini->TxLbwFlowMaxQueueDepth);
            }
         }
      }
      targetChannel = 0;
#endif /* QCA_LL_TX_FLOW_CT */
      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   /*
    * Determine SCC/MSS
    * Remind that this only considered STA+AP and AP+AP concurrency
    * Need to expand for futher concurreny in the future
    */
   if (apChannel1 > 0 && apChannel2 > 0) {
       mcc_mode = apChannel1 != apChannel2;
   } else if (staChannel > 0 && (apChannel1 > 0 || p2pChannel > 0)) {
       mcc_mode = !(p2pChannel==staChannel || apChannel1==staChannel);
   }
   if (pHddCtx->mcc_mode != mcc_mode) {
#ifdef IPA_UC_STA_OFFLOAD
       /* Send SCC/MCC Switching event to IPA */
       hdd_ipa_send_mcc_scc_msg(pHddCtx, mcc_mode);
#endif
       pHddCtx->mcc_mode = mcc_mode;
   }
   hddLog(VOS_TRACE_LEVEL_INFO, "wlan(%d) " MAC_ADDRESS_STR " %s",
                staChannel, MAC_ADDR_ARRAY(staBssid), mcc_mode ? "MCC" : "SCC");
   if (p2pChannel > 0) {
       hddLog(VOS_TRACE_LEVEL_INFO, "p2p-%s(%d) " MAC_ADDRESS_STR,
                     p2pMode, p2pChannel, MAC_ADDR_ARRAY(p2pBssid));
   }
   if (apChannel1 > 0) {
       hddLog(VOS_TRACE_LEVEL_ERROR, "AP(%d) " MAC_ADDRESS_STR,
                     apChannel1, MAC_ADDR_ARRAY(apBssid1));
   }
   if (apChannel2 > 0) {
       hddLog(VOS_TRACE_LEVEL_ERROR, "AP(%d) " MAC_ADDRESS_STR,
                     apChannel2, MAC_ADDR_ARRAY(apBssid2));
   }

   if (p2pChannel > 0 && apChannel1 > 0) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
            FL("Error concurrent SAP %d and P2P %d which is not support"),
            apChannel1, p2pChannel);
   }
}

bool hdd_is_ssr_required( void)
{
    return (isSsrRequired == HDD_SSR_REQUIRED);
}

/* Once SSR is disabled then it cannot be set. */
void hdd_set_ssr_required( e_hdd_ssr_required value)
{
    if (HDD_SSR_DISABLED == isSsrRequired)
        return;

    isSsrRequired = value;
}

VOS_STATUS hdd_get_front_adapter( hdd_context_t *pHddCtx,
                                  hdd_adapter_list_node_t** ppAdapterNode)
{
    VOS_STATUS status;
    spin_lock_bh(&pHddCtx->hddAdapters.lock);
    status =  hdd_list_peek_front ( &pHddCtx->hddAdapters,
                   (hdd_list_node_t**) ppAdapterNode );
    spin_unlock_bh(&pHddCtx->hddAdapters.lock);
    return status;
}

VOS_STATUS hdd_get_next_adapter( hdd_context_t *pHddCtx,
                                 hdd_adapter_list_node_t* pAdapterNode,
                                 hdd_adapter_list_node_t** pNextAdapterNode)
{
    VOS_STATUS status;
    spin_lock_bh(&pHddCtx->hddAdapters.lock);
    status = hdd_list_peek_next ( &pHddCtx->hddAdapters,
                                  (hdd_list_node_t*) pAdapterNode,
                                  (hdd_list_node_t**)pNextAdapterNode );

    spin_unlock_bh(&pHddCtx->hddAdapters.lock);
    return status;
}

VOS_STATUS hdd_remove_adapter( hdd_context_t *pHddCtx,
                               hdd_adapter_list_node_t* pAdapterNode)
{
    VOS_STATUS status;
    spin_lock_bh(&pHddCtx->hddAdapters.lock);
    status =  hdd_list_remove_node ( &pHddCtx->hddAdapters,
                                     &pAdapterNode->node );
    spin_unlock_bh(&pHddCtx->hddAdapters.lock);
    return status;
}

VOS_STATUS hdd_remove_front_adapter( hdd_context_t *pHddCtx,
                                     hdd_adapter_list_node_t** ppAdapterNode)
{
    VOS_STATUS status;
    spin_lock_bh(&pHddCtx->hddAdapters.lock);
    status =  hdd_list_remove_front( &pHddCtx->hddAdapters,
                   (hdd_list_node_t**) ppAdapterNode );
    spin_unlock_bh(&pHddCtx->hddAdapters.lock);
    return status;
}

VOS_STATUS hdd_add_adapter_back( hdd_context_t *pHddCtx,
                                 hdd_adapter_list_node_t* pAdapterNode)
{
    VOS_STATUS status;
    spin_lock_bh(&pHddCtx->hddAdapters.lock);
    status =  hdd_list_insert_back ( &pHddCtx->hddAdapters,
                   (hdd_list_node_t*) pAdapterNode );
    spin_unlock_bh(&pHddCtx->hddAdapters.lock);
    return status;
}

VOS_STATUS hdd_add_adapter_front( hdd_context_t *pHddCtx,
                                  hdd_adapter_list_node_t* pAdapterNode)
{
    VOS_STATUS status;
    spin_lock_bh(&pHddCtx->hddAdapters.lock);
    status =  hdd_list_insert_front ( &pHddCtx->hddAdapters,
                   (hdd_list_node_t*) pAdapterNode );
    spin_unlock_bh(&pHddCtx->hddAdapters.lock);
    return status;
}

hdd_adapter_t * hdd_get_adapter_by_macaddr( hdd_context_t *pHddCtx,
                                            tSirMacAddr macAddr )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   hdd_adapter_t *pAdapter;
   VOS_STATUS status;

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;

      if( pAdapter && vos_mem_compare( pAdapter->macAddressCurrent.bytes,
                                       macAddr, sizeof(tSirMacAddr) ) )
      {
         return pAdapter;
      }
      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   return NULL;

}

hdd_adapter_t * hdd_get_adapter_by_name( hdd_context_t *pHddCtx, tANI_U8 *name )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   hdd_adapter_t *pAdapter;
   VOS_STATUS status;

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;

      if( pAdapter && !strncmp( pAdapter->dev->name, (const char *)name,
          IFNAMSIZ ) )
      {
         return pAdapter;
      }
      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   return NULL;

}

hdd_adapter_t *hdd_get_adapter_by_vdev( hdd_context_t *pHddCtx,
                                        tANI_U32 vdev_id )
{
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
    hdd_adapter_t *pAdapter;
    VOS_STATUS vos_status;


    vos_status = hdd_get_front_adapter( pHddCtx, &pAdapterNode);

    while ((NULL != pAdapterNode) && (VOS_STATUS_SUCCESS == vos_status))
    {
        pAdapter = pAdapterNode->pAdapter;

        if (pAdapter->sessionId == vdev_id)
            return pAdapter;

        vos_status = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pNext);
        pAdapterNode = pNext;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              "%s: vdev_id %d does not exist with host",
              __func__, vdev_id);

    return NULL;
}

/**
 * hdd_get_adapter_by_sme_session_id() - Return adapter with
 * the sessionid
 * @hdd_ctx: hdd cntx.
 * @sme_session_id: sme session is for the adapter to get.
 *
 * This function is used to get the adapter with provided session id
 *
 * Return: adapter pointer if found
 *
 */
hdd_adapter_t *hdd_get_adapter_by_sme_session_id(hdd_context_t *hdd_ctx,
						uint32_t sme_session_id)
{
	hdd_adapter_list_node_t *adapter_node = NULL, *next = NULL;
	hdd_adapter_t *adapter;
	VOS_STATUS vos_status;


	vos_status = hdd_get_front_adapter(hdd_ctx, &adapter_node);

	while ((NULL != adapter_node) &&
			(VOS_STATUS_SUCCESS == vos_status)) {
		adapter = adapter_node->pAdapter;

		if (adapter &&
			 adapter->sessionId == sme_session_id)
			return adapter;

		vos_status =
			hdd_get_next_adapter(hdd_ctx,
				 adapter_node, &next);
		adapter_node = next;
	}
	return NULL;
}

hdd_adapter_t * hdd_get_adapter( hdd_context_t *pHddCtx, device_mode_t mode )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   hdd_adapter_t *pAdapter;
   VOS_STATUS status;

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;

      if( pAdapter && (mode == pAdapter->device_mode) )
      {
         return pAdapter;
      }
      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   return NULL;

}

//Remove this function later
hdd_adapter_t * hdd_get_mon_adapter( hdd_context_t *pHddCtx )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   hdd_adapter_t *pAdapter;
   VOS_STATUS status;

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;

      if( pAdapter && WLAN_HDD_MONITOR == pAdapter->device_mode )
      {
         return pAdapter;
      }

      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   return NULL;

}

/**---------------------------------------------------------------------------

  \brief hdd_set_monitor_tx_adapter() -

   This API initializes the adapter to be used while transmitting on monitor
   adapter.

  \param  - pHddCtx - Pointer to the HDD context.
            pAdapter - Adapter that will used for TX. This can be NULL.
  \return - None.
  --------------------------------------------------------------------------*/
void wlan_hdd_set_monitor_tx_adapter( hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter )
{
   hdd_adapter_t *pMonAdapter;

   pMonAdapter = hdd_get_adapter( pHddCtx, WLAN_HDD_MONITOR );

   if( NULL != pMonAdapter )
   {
      pMonAdapter->sessionCtx.monitor.pAdapterForTx = pAdapter;
   }
}
/**---------------------------------------------------------------------------

  \brief hdd_get_operating_channel() -

   This API returns the operating channel of the requested device mode

  \param  - pHddCtx - Pointer to the HDD context.
              - mode - Device mode for which operating channel is required
                supported modes - WLAN_HDD_INFRA_STATION, WLAN_HDD_P2P_CLIENT
                                 WLAN_HDD_SOFTAP, WLAN_HDD_P2P_GO.
  \return - channel number. "0" id the requested device is not found OR it is not connected.
  --------------------------------------------------------------------------*/
v_U8_t hdd_get_operating_channel( hdd_context_t *pHddCtx, device_mode_t mode )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   hdd_adapter_t      *pAdapter;
   v_U8_t operatingChannel = 0;

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;

      if( mode == pAdapter->device_mode )
      {
        switch(pAdapter->device_mode)
        {
          case WLAN_HDD_INFRA_STATION:
          case WLAN_HDD_P2P_CLIENT:
            if( hdd_connIsConnected( WLAN_HDD_GET_STATION_CTX_PTR( pAdapter )) )
              operatingChannel = (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.operationChannel;
            break;
          case WLAN_HDD_SOFTAP:
          case WLAN_HDD_P2P_GO:
            /*softap connection info */
            if(test_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags))
              operatingChannel = (WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->operatingChannel;
            break;
          default:
            break;
        }

        break; //Found the device of interest. break the loop
      }

      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }
   return operatingChannel;
}

/**---------------------------------------------------------------------------

  \brief hdd_wlan_initial_scan() -

   This function triggers the initial scan

  \param  - pAdapter - Pointer to the HDD adapter.

  --------------------------------------------------------------------------*/
void hdd_wlan_initial_scan(hdd_adapter_t *pAdapter)
{
   tCsrScanRequest scanReq;
   tCsrChannelInfo channelInfo;
   eHalStatus halStatus;
   tANI_U32 scanId;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

   vos_mem_zero(&scanReq, sizeof(tCsrScanRequest));
   vos_mem_set(&scanReq.bssid, sizeof(tCsrBssid), 0xff);
   scanReq.BSSType = eCSR_BSS_TYPE_ANY;

   if(sme_Is11dSupported(pHddCtx->hHal))
   {
      halStatus = sme_ScanGetBaseChannels( pHddCtx->hHal, &channelInfo );
      if ( HAL_STATUS_SUCCESS( halStatus ) )
      {
         scanReq.ChannelInfo.ChannelList = vos_mem_malloc(channelInfo.numOfChannels);
         if( !scanReq.ChannelInfo.ChannelList )
         {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s kmalloc failed", __func__);
            vos_mem_free(channelInfo.ChannelList);
            channelInfo.ChannelList = NULL;
            return;
         }
         vos_mem_copy(scanReq.ChannelInfo.ChannelList, channelInfo.ChannelList,
            channelInfo.numOfChannels);
         scanReq.ChannelInfo.numOfChannels = channelInfo.numOfChannels;
         vos_mem_free(channelInfo.ChannelList);
         channelInfo.ChannelList = NULL;
      }

      scanReq.scanType = eSIR_PASSIVE_SCAN;
      scanReq.requestType = eCSR_SCAN_REQUEST_11D_SCAN;
      scanReq.maxChnTime = pHddCtx->cfg_ini->nPassiveMaxChnTime;
      scanReq.minChnTime = pHddCtx->cfg_ini->nPassiveMinChnTime;
   }
   else
   {
      scanReq.scanType = eSIR_ACTIVE_SCAN;
      scanReq.requestType = eCSR_SCAN_REQUEST_FULL_SCAN;
      scanReq.maxChnTime = pHddCtx->cfg_ini->nActiveMaxChnTime;
      scanReq.minChnTime = pHddCtx->cfg_ini->nActiveMinChnTime;
   }

   halStatus = sme_ScanRequest(pHddCtx->hHal, pAdapter->sessionId, &scanReq, &scanId, NULL, NULL);
   if ( !HAL_STATUS_SUCCESS( halStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: sme_ScanRequest failed status code %d",
         __func__, halStatus );
   }

   if(sme_Is11dSupported(pHddCtx->hHal))
        vos_mem_free(scanReq.ChannelInfo.ChannelList);
}

/**---------------------------------------------------------------------------

  \brief hdd_full_power_callback() - HDD full power callback function

  This is the function invoked by SME to inform the result of a full power
  request issued by HDD

  \param  - callback context - Pointer to cookie
  \param  - status - result of request

  \return - None

  --------------------------------------------------------------------------*/
static void hdd_full_power_callback(void *callbackContext, eHalStatus status)
{
   struct statsContext *pContext = callbackContext;

   hddLog(VOS_TRACE_LEVEL_INFO,
          "%s: context = %p, status = %d", __func__, pContext, status);

   if (NULL == callbackContext)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Bad param, context [%p]",
             __func__, callbackContext);
      return;
   }

   /* there is a race condition that exists between this callback
      function and the caller since the caller could time out either
      before or while this code is executing.  we use a spinlock to
      serialize these actions */
   spin_lock(&hdd_context_lock);

   if (POWER_CONTEXT_MAGIC != pContext->magic)
   {
      /* the caller presumably timed out so there is nothing we can do */
      spin_unlock(&hdd_context_lock);
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: Invalid context, magic [%08x]",
              __func__, pContext->magic);
      return;
   }

   /* context is valid so caller is still waiting */

   /* paranoia: invalidate the magic */
   pContext->magic = 0;

   /* notify the caller */
   complete(&pContext->completion);

   /* serialization is complete */
   spin_unlock(&hdd_context_lock);
}

static inline VOS_STATUS hdd_UnregisterWext_all_adapters(hdd_context_t *pHddCtx)
{
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
    VOS_STATUS status;
    hdd_adapter_t      *pAdapter;

    ENTER();

    status = hdd_get_front_adapter(pHddCtx, &pAdapterNode);

    while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
    {
        pAdapter = pAdapterNode->pAdapter;
        if ((pAdapter->device_mode == WLAN_HDD_INFRA_STATION) ||
                (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT) ||
                (pAdapter->device_mode == WLAN_HDD_IBSS) ||
                (pAdapter->device_mode == WLAN_HDD_P2P_DEVICE) ||
                (pAdapter->device_mode == WLAN_HDD_SOFTAP) ||
                (pAdapter->device_mode == WLAN_HDD_P2P_GO)) {
            wlan_hdd_cfg80211_deregister_frames(pAdapter);
            hdd_UnregisterWext(pAdapter->dev);
        }
        status = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pNext);
        pAdapterNode = pNext;
    }

    EXIT();

    return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_abort_mac_scan_all_adapters(hdd_context_t *pHddCtx)
{
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
    VOS_STATUS status;
    hdd_adapter_t      *pAdapter;

    ENTER();

    status = hdd_get_front_adapter(pHddCtx, &pAdapterNode);

    while (NULL != pAdapterNode && VOS_STATUS_SUCCESS == status)
    {
        pAdapter = pAdapterNode->pAdapter;
        if ((pAdapter->device_mode == WLAN_HDD_INFRA_STATION) ||
            (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT) ||
            (pAdapter->device_mode == WLAN_HDD_IBSS) ||
            (pAdapter->device_mode == WLAN_HDD_P2P_DEVICE) ||
            (pAdapter->device_mode == WLAN_HDD_SOFTAP) ||
            (pAdapter->device_mode == WLAN_HDD_P2P_GO)) {
            hdd_abort_mac_scan(pHddCtx, pAdapter->sessionId,
                               eCSR_SCAN_ABORT_DEFAULT);
        }
        status = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pNext);
        pAdapterNode = pNext;
    }

    EXIT();

    return VOS_STATUS_SUCCESS;
}

/**
 * wlan_hdd_restart_init() - restart init
 * @pHddCtx:  Pointer to hdd context
 *
 * This function initializes restart timer/flag. An internal function.
 *
 * Return: None
 */
static void wlan_hdd_restart_init(hdd_context_t *pHddCtx)
{
   /* Initialize */
   pHddCtx->hdd_restart_retries = 0;
   atomic_set(&pHddCtx->isRestartInProgress, 0);
   vos_timer_init(&pHddCtx->hdd_restart_timer,
                     VOS_TIMER_TYPE_SW,
                     wlan_hdd_restart_timer_cb,
                     pHddCtx);
}

/**
 * wlan_hdd_restart_deinit() - restart deinit
 * @pHddCtx:  Pointer to hdd context
 *
 * This function cleans up the resources used. An internal function.
 *
 * Return: None
 */
static void wlan_hdd_restart_deinit(hdd_context_t* pHddCtx)
{
   VOS_STATUS vos_status;
   /* Block any further calls */
   atomic_set(&pHddCtx->isRestartInProgress, 1);
   /* Cleanup */
   vos_status = vos_timer_stop( &pHddCtx->hdd_restart_timer );
   if (!VOS_IS_STATUS_SUCCESS(vos_status))
          hddLog(LOGE, FL("Failed to stop HDD restart timer"));
   vos_status = vos_timer_destroy(&pHddCtx->hdd_restart_timer);
   if (!VOS_IS_STATUS_SUCCESS(vos_status))
          hddLog(LOGE, FL("Failed to destroy HDD restart timer"));
}

#ifdef WLAN_NS_OFFLOAD
/**
 * hdd_wlan_unregister_ip6_notifier() - unregister IP6 change notifier
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: None
 */
static void hdd_wlan_unregister_ip6_notifier(hdd_context_t *hdd_ctx)
{
	unregister_inet6addr_notifier(&hdd_ctx->ipv6_notifier);

	return;
}

/**
 * hdd_wlan_register_ip6_notifier() - register IP6 change notifier
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: None
 */
static void hdd_wlan_register_ip6_notifier(hdd_context_t *hdd_ctx)
{
	int ret;

	hdd_ctx->ipv6_notifier.notifier_call = wlan_hdd_ipv6_changed;
	ret = register_inet6addr_notifier(&hdd_ctx->ipv6_notifier);
	if (ret)
		hddLog(LOGE, FL("Failed to register IPv6 notifier"));
	else
		hddLog(LOG1, FL("Registered IPv6 notifier"));

	return;
}
#else
/**
 * hdd_wlan_unregister_ip6_notifier() - unregister IP6 change notifier
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: None
 */
static void hdd_wlan_unregister_ip6_notifier(hdd_context_t *hdd_ctx)
{
}
/**
 * hdd_wlan_register_ip6_notifier() - register IP6 change notifier
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: None
 */
static void hdd_wlan_register_ip6_notifier(hdd_context_t *hdd_ctx)
{
}
#endif

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
/**
 * wlan_hdd_logging_sock_activate_svc() - Activate logging
 * @hdd_ctx: HDD context
 *
 * Activates the logging service
 *
 * Return: Zero in case of success, negative value otherwise
 */
static int wlan_hdd_logging_sock_activate_svc(hdd_context_t *hdd_ctx)
{
	if (hdd_ctx->cfg_ini->wlanLoggingEnable) {
		if (wlan_logging_sock_activate_svc(
				hdd_ctx->cfg_ini->wlanLoggingFEToConsole,
				hdd_ctx->cfg_ini->wlanLoggingNumBuf)) {
			hddLog(VOS_TRACE_LEVEL_ERROR,
				"%s: wlan_logging_sock_activate_svc failed",
				__func__);
			return -EINVAL;
		}
	}
	return 0;
}
/**
 * wlan_hdd_logging_sock_deactivate_svc() - Deactivate logging
 * @hdd_ctx: HDD context
 *
 * Deactivates the logging service
 *
 * Return: 0 on deactivating the logging service
 */
static int wlan_hdd_logging_sock_deactivate_svc(hdd_context_t *hdd_ctx)
{
	if (hdd_ctx && hdd_ctx->cfg_ini->wlanLoggingEnable)
		return wlan_logging_sock_deactivate_svc();

	return 0;
}
#else
static inline int wlan_hdd_logging_sock_activate_svc(hdd_context_t *hdd_ctx)
{
	return 0;
}

static inline int wlan_hdd_logging_sock_deactivate_svc(hdd_context_t *hdd_ctx)
{
	return 0;
}
#endif

/**---------------------------------------------------------------------------

  \brief hdd_wlan_exit() - HDD WLAN exit function

  This is the driver exit point (invoked during rmmod)

  \param  - pHddCtx - Pointer to the HDD Context

  \return - None

  --------------------------------------------------------------------------*/
void hdd_wlan_exit(hdd_context_t *pHddCtx)
{
   eHalStatus halStatus;
   v_CONTEXT_t pVosContext = pHddCtx->pvosContext;
   VOS_STATUS vosStatus;
   struct wiphy *wiphy = pHddCtx->wiphy;
   struct statsContext powerContext;
   unsigned long rc;
   hdd_config_t *pConfig = pHddCtx->cfg_ini;

   ENTER();

   hddLog(LOGE, FL("Unregister IPv6 notifier"));
   hdd_wlan_unregister_ip6_notifier(pHddCtx);
   hddLog(LOGE, FL("Unregister IPv4 notifier"));
   unregister_inetaddr_notifier(&pHddCtx->ipv4_notifier);

   if (VOS_FTM_MODE != hdd_get_conparam())
   {
      // Unloading, restart logic is no more required.
      wlan_hdd_restart_deinit(pHddCtx);
   }

   hdd_UnregisterWext_all_adapters(pHddCtx);

   if (VOS_FTM_MODE == hdd_get_conparam())
   {
      hddLog(VOS_TRACE_LEVEL_INFO, "%s: FTM MODE", __func__);
#if  defined(QCA_WIFI_FTM)
      if (hdd_ftm_stop(pHddCtx))
      {
          hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hdd_ftm_stop Failed",__func__);
          VOS_ASSERT(0);
      }
      pHddCtx->ftm.ftm_state = WLAN_FTM_STOPPED;
#endif
      wlan_hdd_ftm_close(pHddCtx);
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: FTM driver unloaded", __func__);
      goto free_hdd_ctx;
   }

   /* DeRegister with platform driver as client for Suspend/Resume */
   vosStatus = hddDeregisterPmOps(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hddDeregisterPmOps failed",__func__);
      VOS_ASSERT(0);
   }

   vosStatus = hddDevTmUnregisterNotifyCallback(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hddDevTmUnregisterNotifyCallback failed",__func__);
   }

   /*
    * Cancel any outstanding scan requests.  We are about to close all
    * of our adapters, but an adapter structure is what SME passes back
    * to our callback function.  Hence if there are any outstanding scan
    * requests then there is a race condition between when the adapter
    * is closed and when the callback is invoked.  We try to resolve that
    * race condition here by cancelling any outstanding scans before we
    * close the adapters.
    * Note that the scans may be cancelled in an asynchronous manner, so
    * ideally there needs to be some kind of synchronization.  Rather than
    * introduce a new synchronization here, we will utilize the fact that
    * we are about to Request Full Power, and since that is synchronized,
    * the expectation is that by the time Request Full Power has completed,
    * all scans will be cancelled.
    */
   hdd_abort_mac_scan_all_adapters(pHddCtx);

#ifdef FEATURE_BUS_BANDWIDTH
   if (VOS_TIMER_STATE_RUNNING ==
                        vos_timer_getCurrentState(&pHddCtx->bus_bw_timer))
   {
      vos_timer_stop(&pHddCtx->bus_bw_timer);
   }

   if (!VOS_IS_STATUS_SUCCESS(vos_timer_destroy(
                         &pHddCtx->bus_bw_timer)))
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
            "%s: Cannot deallocate Bus bandwidth timer", __func__);
   }
#endif

#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
   if (VOS_TIMER_STATE_RUNNING ==
                vos_timer_getCurrentState(&pHddCtx->skip_acs_scan_timer)) {
       vos_timer_stop(&pHddCtx->skip_acs_scan_timer);
   }

   if (!VOS_IS_STATUS_SUCCESS(vos_timer_destroy(
                         &pHddCtx->skip_acs_scan_timer))) {
       hddLog(VOS_TRACE_LEVEL_ERROR,
            "%s: Cannot deallocate ACS Skip timer", __func__);
   }
#endif

   if (pConfig && !pConfig->enablePowersaveOffload)
   {
      //Disable IMPS/BMPS as we do not want the device to enter any power
      //save mode during shutdown
      sme_DisablePowerSave(pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
      sme_DisablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);
      sme_DisablePowerSave(pHddCtx->hHal, ePMC_UAPSD_MODE_POWER_SAVE);

      //Ensure that device is in full power as we will touch H/W during vos_Stop
      init_completion(&powerContext.completion);
      powerContext.magic = POWER_CONTEXT_MAGIC;

      halStatus = sme_RequestFullPower(pHddCtx->hHal, hdd_full_power_callback,
            &powerContext, eSME_FULL_PWR_NEEDED_BY_HDD);

      if (eHAL_STATUS_SUCCESS != halStatus)
      {
         if (eHAL_STATUS_PMC_PENDING == halStatus)
         {
            /* request was sent -- wait for the response */
            rc = wait_for_completion_timeout(
                  &powerContext.completion,
                  msecs_to_jiffies(WLAN_WAIT_TIME_POWER));
            if (!rc) {
               hddLog(VOS_TRACE_LEVEL_ERROR,
                     "%s: timed out while requesting full power",
                     __func__);
            }
         }
         else
         {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                  "%s: Request for Full Power failed, status %d",
                  __func__, halStatus);
            /* continue -- need to clean up as much as possible */
         }
      }
      /* either we never sent a request, we sent a request and received a
         response or we sent a request and timed out.  if we never sent a
         request or if we sent a request and got a response, we want to
         clear the magic out of paranoia.  if we timed out there is a
         race condition such that the callback function could be
         executing at the same time we are. of primary concern is if the
         callback function had already verified the "magic" but had not
         yet set the completion variable when a timeout occurred. we
         serialize these activities by invalidating the magic while
         holding a shared spinlock which will cause us to block if the
         callback is currently executing */
      spin_lock(&hdd_context_lock);
      powerContext.magic = 0;
      spin_unlock(&hdd_context_lock);
   }
   else
   {
      /*
       * Powersave Offload Case
       * Disable Idle Power Save Mode
       */
      hdd_set_idle_ps_config(pHddCtx, FALSE);
   }

   hdd_debugfs_exit(pHddCtx);

   // Unregister the Net Device Notifier
   unregister_netdevice_notifier(&hdd_netdev_notifier);

   /* Stop all adapters, this will ensure the termination of active
    * connections on the interface. Make sure the vos_scheduler is
    * still available to handle those control messages
    */
   hdd_stop_all_adapters( pHddCtx );


   //Stop all the modules
   vosStatus = vos_stop( pVosContext );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
         "%s: Failed to stop VOSS",__func__);
      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   }

   //This requires pMac access, Call this before vos_close().
   hdd_unregister_mcast_bcast_filter(pHddCtx);

   //Close the scheduler before calling vos_close to make sure no thread is
   // scheduled after the each module close is called i.e after all the data
   // structures are freed.
   vosStatus = vos_sched_close( pVosContext );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))    {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
         "%s: Failed to close VOSS Scheduler",__func__);
      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   }
#ifdef WLAN_FEATURE_HOLD_RX_WAKELOCK
   /* Destroy the wake lock */
   vos_wake_lock_destroy(&pHddCtx->rx_wake_lock);
#endif
   /* Destroy the wake lock */
   vos_wake_lock_destroy(&pHddCtx->sap_wake_lock);

   hdd_hostapd_channel_wakelock_deinit(pHddCtx);

  vosStatus = vos_nv_close();
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close NV", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

   //Close VOSS
   //This frees pMac(HAL) context. There should not be any call that requires pMac access after this.
   vos_close(pVosContext);

   hdd_wlan_green_ap_deinit(pHddCtx);

   //Close Watchdog
   if (pConfig && pConfig->fIsLogpEnabled)
      vos_watchdog_close(pVosContext);

   //Clean up HDD Nlink Service
   send_btc_nlink_msg(WLAN_MODULE_DOWN_IND, 0);

   if (VOS_FTM_MODE != hdd_get_conparam())
       wlan_hdd_logging_sock_deactivate_svc(pHddCtx);

#ifdef WLAN_KD_READY_NOTIFIER
   cnss_diag_notify_wlan_close();
   nl_srv_exit(pHddCtx->ptt_pid);
#else
   nl_srv_exit();
#endif /* WLAN_KD_READY_NOTIFIER */

   hdd_close_cesium_nl_sock();

   hdd_runtime_suspend_deinit(pHddCtx);
   hdd_close_all_adapters( pHddCtx );

#ifdef IPA_OFFLOAD
   hdd_ipa_cleanup(pHddCtx);
#endif

   /* free the power on lock from platform driver */
   if (free_riva_power_on_lock("wlan"))
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to free power on lock",
                                           __func__);
   }

   /* Free up RoC request queue and flush workqueue */
   vos_flush_work(&pHddCtx->rocReqWork);
   hdd_list_destroy(&pHddCtx->hdd_roc_req_q);

free_hdd_ctx:

   /* Free up dynamically allocated members inside HDD Adapter */
   if (pHddCtx->cfg_ini) {
       kfree(pHddCtx->cfg_ini);
       pHddCtx->cfg_ini= NULL;
   }

   wlan_hdd_deinit_tx_rx_histogram(pHddCtx);
   wiphy_unregister(wiphy) ;

   wiphy_free(wiphy) ;
   if (hdd_is_ssr_required())
   {
#ifdef MSM_PLATFORM
#ifdef CONFIG_CNSS
       /* WDI timeout had happened during unload, so SSR is needed here */
       subsystem_restart("wcnss");
#endif
#endif
       msleep(5000);
   }
   hdd_set_ssr_required (VOS_FALSE);
}

void __hdd_wlan_exit(void)
{
   hdd_context_t *pHddCtx = NULL;
   v_CONTEXT_t pVosContext = NULL;

   ENTER();

   //Get the global vos context
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   if (!pVosContext)
      return;
   if (WLAN_IS_EPPING_ENABLED(con_mode)) {
      epping_exit(pVosContext);
      return;
   }

   if(NULL == pVosContext) {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
            "%s:Invalid global VOSS context", __func__);
      EXIT();
      return;
   }

   //Get the HDD context.
   pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD,
         pVosContext);

   if(NULL == pHddCtx) {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
            "%s:Invalid HDD Context", __func__);
      EXIT();
      return;
   }

   /* module exit should never proceed if SSR is not completed */
   while(pHddCtx->isLogpInProgress){
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
            "%s:SSR in Progress; block rmmod for 1 second!!!",
            __func__);
      msleep(1000);
   }

   pHddCtx->isUnloadInProgress = TRUE;

   vos_set_load_unload_in_progress(VOS_MODULE_ID_VOSS, TRUE);
   vos_set_unload_in_progress(TRUE);

#ifdef WLAN_FEATURE_LPSS
   wlan_hdd_send_status_pkg(NULL, NULL, 0, 0);
#endif

   //Do all the cleanup before deregistering the driver
   memdump_deinit();
   hdd_wlan_exit(pHddCtx);
   EXIT();
}

#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
void hdd_skip_acs_scan_timer_handler(void * data)
{
    hdd_context_t *hdd_ctx = (hdd_context_t *) data;
    hdd_adapter_t *ap_adapter;

    hddLog(LOG1, FL("ACS Scan result expired. Reset ACS scan skip"));
    hdd_ctx->skip_acs_scan_status = eSAP_DO_NEW_ACS_SCAN;

    /* Get first SAP adapter to clear results */
    ap_adapter = hdd_get_adapter(hdd_ctx, WLAN_HDD_SOFTAP);
    if (!hdd_ctx->hHal || !ap_adapter)
        return;
    sme_ScanFlushResult(hdd_ctx->hHal, ap_adapter->sessionId);
}
#endif

#ifdef QCA_HT_2040_COEX
/**--------------------------------------------------------------------------

  \brief notify FW with HT20/HT40 mode

  -------------------------------------------------------------------------*/
int hdd_wlan_set_ht2040_mode(hdd_adapter_t *pAdapter, v_U16_t staId,
                             v_MACADDR_t macAddrSTA, int channel_type)
{
   int status;
   VOS_STATUS vosStatus;
   hdd_context_t *pHddCtx = NULL;

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

   status = wlan_hdd_validate_context(pHddCtx);
   if (0 != status)
   {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: HDD context is not valid", __func__);
       return -1;
   }
   if (!pHddCtx->hHal)
      return -1;

   vosStatus = sme_notify_ht2040_mode(pHddCtx->hHal, staId, macAddrSTA,
                                      pAdapter->sessionId, channel_type);
   if (VOS_STATUS_SUCCESS != vosStatus) {
      hddLog(LOGE, "Fail to send notification with ht2040 mode\n");
      return -1;
   }

   return 0;
}
#endif


/**--------------------------------------------------------------------------

  \brief notify FW with modem power status

  -------------------------------------------------------------------------*/
int hdd_wlan_notify_modem_power_state(int state)
{
   int status;
   VOS_STATUS vosStatus;
   v_CONTEXT_t pVosContext = NULL;
   hdd_context_t *pHddCtx = NULL;

   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   if (!pVosContext)
      return -1;

   pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext);

   status = wlan_hdd_validate_context(pHddCtx);
   if (0 != status)
   {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: HDD context is not valid", __func__);
       return -1;
   }
   if (!pHddCtx->hHal)
      return -1;

   vosStatus = sme_notify_modem_power_state(pHddCtx->hHal, state);
   if (VOS_STATUS_SUCCESS != vosStatus) {
      hddLog(LOGE, "Fail to send notification with modem power state %d",
             state);
      return -1;
   }
   return 0;
}

/**---------------------------------------------------------------------------

  \brief hdd_post_voss_start_config() - HDD post voss start config helper

  \param  - pAdapter - Pointer to the HDD

  \return - None

  --------------------------------------------------------------------------*/
VOS_STATUS hdd_post_voss_start_config(hdd_context_t* pHddCtx)
{
   eHalStatus halStatus;
   v_U32_t listenInterval;
   tANI_U32    ignoreDtim;


   // Send ready indication to the HDD.  This will kick off the MAC
   // into a 'running' state and should kick off an initial scan.
   halStatus = sme_HDDReadyInd( pHddCtx->hHal );
   if ( !HAL_STATUS_SUCCESS( halStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: sme_HDDReadyInd() failed with status "
          "code %08d [x%08x]",__func__, halStatus, halStatus );
      return VOS_STATUS_E_FAILURE;
   }

   // Set default LI and ignoreDtim into HDD context,
   // otherwise under some race condition, HDD will set 0 LI value into RIVA,
   // And RIVA will crash
   wlan_cfgGetInt(pHddCtx->hHal, WNI_CFG_LISTEN_INTERVAL, &listenInterval);
   pHddCtx->hdd_actual_LI_value = listenInterval;
   wlan_cfgGetInt(pHddCtx->hHal, WNI_CFG_IGNORE_DTIM, &ignoreDtim);
   pHddCtx->hdd_actual_ignore_DTIM_value = ignoreDtim;


   return VOS_STATUS_SUCCESS;
}

/* wake lock APIs for HDD */
void hdd_prevent_suspend(uint32_t reason)
{
	vos_wake_lock_acquire(&wlan_wake_lock, reason);
}

void hdd_allow_suspend(uint32_t reason)
{
	vos_wake_lock_release(&wlan_wake_lock, reason);
}

void hdd_prevent_suspend_timeout(v_U32_t timeout, uint32_t reason)
{
	vos_wake_lock_timeout_acquire(&wlan_wake_lock, timeout,
                                      reason);
}

/**---------------------------------------------------------------------------

  \brief hdd_exchange_version_and_caps() - HDD function to exchange version and capability
                                                                 information between Host and Riva

  This function gets reported version of FW
  It also finds the version of Riva headers used to compile the host
  It compares the above two and prints a warning if they are different
  It gets the SW and HW version string
  Finally, it exchanges capabilities between host and Riva i.e. host and riva exchange a msg
  indicating the features they support through a bitmap

  \param  - pHddCtx - Pointer to HDD context

  \return -  void

  --------------------------------------------------------------------------*/

void hdd_exchange_version_and_caps(hdd_context_t *pHddCtx)
{

   tSirVersionType versionCompiled;
   tSirVersionType versionReported;
   tSirVersionString versionString;
   tANI_U8 fwFeatCapsMsgSupported = 0;
   VOS_STATUS vstatus;

   memset(&versionCompiled, 0, sizeof(versionCompiled));
   memset(&versionReported, 0, sizeof(versionReported));

   /* retrieve and display WCNSS version information */
   do {

      vstatus = sme_GetWcnssWlanCompiledVersion(pHddCtx->hHal,
                                                &versionCompiled);
      if (!VOS_IS_STATUS_SUCCESS(vstatus))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: unable to retrieve WCNSS WLAN compiled version",
                __func__);
         break;
      }

      vstatus = sme_GetWcnssWlanReportedVersion(pHddCtx->hHal,
                                                &versionReported);
      if (!VOS_IS_STATUS_SUCCESS(vstatus))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: unable to retrieve WCNSS WLAN reported version",
                __func__);
         break;
      }

      if ((versionCompiled.major != versionReported.major) ||
          (versionCompiled.minor != versionReported.minor) ||
          (versionCompiled.version != versionReported.version) ||
          (versionCompiled.revision != versionReported.revision))
      {
         pr_err("%s: WCNSS WLAN Version %u.%u.%u.%u, "
                "Host expected %u.%u.%u.%u\n",
                WLAN_MODULE_NAME,
                (int)versionReported.major,
                (int)versionReported.minor,
                (int)versionReported.version,
                (int)versionReported.revision,
                (int)versionCompiled.major,
                (int)versionCompiled.minor,
                (int)versionCompiled.version,
                (int)versionCompiled.revision);
      }
      else
      {
         pr_info("%s: WCNSS WLAN version %u.%u.%u.%u\n",
                 WLAN_MODULE_NAME,
                 (int)versionReported.major,
                 (int)versionReported.minor,
                 (int)versionReported.version,
                 (int)versionReported.revision);
      }

      vstatus = sme_GetWcnssSoftwareVersion(pHddCtx->hHal,
                                            versionString,
                                            sizeof(versionString));
      if (!VOS_IS_STATUS_SUCCESS(vstatus))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: unable to retrieve WCNSS software version string",
                __func__);
         break;
      }

      pr_info("%s: WCNSS software version %s\n",
              WLAN_MODULE_NAME, versionString);

      vstatus = sme_GetWcnssHardwareVersion(pHddCtx->hHal,
                                            versionString,
                                            sizeof(versionString));
      if (!VOS_IS_STATUS_SUCCESS(vstatus))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: unable to retrieve WCNSS hardware version string",
                __func__);
         break;
      }

      pr_info("%s: WCNSS hardware version %s\n",
              WLAN_MODULE_NAME, versionString);

      /* 1.Check if FW version is greater than 0.1.1.0. Only then send host-FW capability exchange message
         2.Host-FW capability exchange message  is only present on riva 1.1 so
            send the message only if it the riva is 1.1
            minor numbers for different riva branches:
                0 -> (1.0)Mainline Build
                1 -> (1.1)Mainline Build
                2->(1.04) Stability Build
       */
      if (((versionReported.major>0) || (versionReported.minor>1) ||
         ((versionReported.minor>=1) && (versionReported.version>=1)))
         && ((versionReported.major == 1) && (versionReported.minor >= 1)))
         fwFeatCapsMsgSupported = 1;

      if (fwFeatCapsMsgSupported)
      {
#ifdef WLAN_ACTIVEMODE_OFFLOAD_FEATURE
         if(!pHddCtx->cfg_ini->fEnableActiveModeOffload)
            sme_disableFeatureCapablity(WLANACTIVE_OFFLOAD);
#endif
         /* Indicate if IBSS heartbeat monitoring needs to be offloaded */
         if (!pHddCtx->cfg_ini->enableIbssHeartBeatOffload)
         {
            sme_disableFeatureCapablity(IBSS_HEARTBEAT_OFFLOAD);
         }

         sme_featureCapsExchange(pHddCtx->hHal);
      }

   } while (0);

}

/* Initialize channel list in sme based on the country code */
VOS_STATUS hdd_set_sme_chan_list(hdd_context_t *hdd_ctx)
{
    return sme_init_chan_list(hdd_ctx->hHal, hdd_ctx->reg.alpha2,
                              hdd_ctx->reg.cc_src);
}

/**---------------------------------------------------------------------------

  \brief hdd_is_5g_supported() - HDD function to know if hardware supports  5GHz

  \param  - pHddCtx - Pointer to the hdd context

  \return -  true if hardware supports 5GHz

  --------------------------------------------------------------------------*/
boolean hdd_is_5g_supported(hdd_context_t * pHddCtx)
{
   /* If wcnss_wlan_iris_xo_mode() returns WCNSS_XO_48MHZ(1);
    * then hardware support 5Ghz.
   */
   return true;
}

static VOS_STATUS wlan_hdd_reg_init(hdd_context_t *hdd_ctx)
{
   struct wiphy *wiphy;
   VOS_STATUS status = VOS_STATUS_SUCCESS;

   wiphy = hdd_ctx->wiphy;

   /* initialize the NV module. This is required so that
      we can initialize the channel information in wiphy
      from the NV.bin data. The channel information in
      wiphy needs to be initialized before wiphy registration */

   status = vos_init_wiphy_from_eeprom();
   if (!VOS_IS_STATUS_SUCCESS(status))
   {
      /* NV module cannot be initialized */
      hddLog( VOS_TRACE_LEVEL_FATAL,
            "%s: vos_init_wiphy failed", __func__);
      return status;
   }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0)) || defined(WITH_BACKPORTS)
    wiphy->wowlan = &wowlan_support_reg_init;
#else
    wiphy->wowlan.flags = WIPHY_WOWLAN_ANY |
                          WIPHY_WOWLAN_MAGIC_PKT |
                          WIPHY_WOWLAN_DISCONNECT |
                          WIPHY_WOWLAN_SUPPORTS_GTK_REKEY |
                          WIPHY_WOWLAN_GTK_REKEY_FAILURE |
                          WIPHY_WOWLAN_EAP_IDENTITY_REQ |
                          WIPHY_WOWLAN_4WAY_HANDSHAKE |
                          WIPHY_WOWLAN_RFKILL_RELEASE;

    wiphy->wowlan.n_patterns = (WOW_MAX_FILTER_LISTS *
                          WOW_MAX_FILTERS_PER_LIST);
    wiphy->wowlan.pattern_min_len = WOW_MIN_PATTERN_SIZE;
    wiphy->wowlan.pattern_max_len = WOW_MAX_PATTERN_SIZE;
#endif

   /* registration of wiphy dev with cfg80211 */
   if (0 > wlan_hdd_cfg80211_register(wiphy))
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: wiphy register failed", __func__);
      status = VOS_STATUS_E_FAILURE;
   }

   return status;
}


#ifdef FEATURE_BUS_BANDWIDTH
void hdd_cnss_request_bus_bandwidth(hdd_context_t *pHddCtx,
        const uint64_t tx_packets, const uint64_t rx_packets)
{
    uint64_t total = tx_packets + rx_packets;
    uint64_t temp_rx = 0;
    uint64_t temp_tx = 0;
    enum cnss_bus_width_type next_vote_level = CNSS_BUS_WIDTH_NONE;
    enum wlan_tp_level next_rx_level = WLAN_SVC_TP_NONE;
    enum wlan_tp_level next_tx_level = WLAN_SVC_TP_NONE;


    if (total > pHddCtx->cfg_ini->busBandwidthHighThreshold)
        next_vote_level = CNSS_BUS_WIDTH_HIGH;
    else if (total > pHddCtx->cfg_ini->busBandwidthMediumThreshold)
        next_vote_level = CNSS_BUS_WIDTH_MEDIUM;
    else if (total > pHddCtx->cfg_ini->busBandwidthLowThreshold)
        next_vote_level = CNSS_BUS_WIDTH_LOW;
    else
        next_vote_level = CNSS_BUS_WIDTH_NONE;

    pHddCtx->hdd_txrx_hist[pHddCtx->hdd_txrx_hist_idx].next_vote_level
                                                            = next_vote_level;

    if (pHddCtx->cur_vote_level != next_vote_level) {
        hddLog(VOS_TRACE_LEVEL_DEBUG,
               "%s: trigger level %d, tx_packets: %lld, rx_packets: %lld",
               __func__, next_vote_level, tx_packets, rx_packets);
        pHddCtx->cur_vote_level = next_vote_level;
        vos_request_bus_bandwidth(next_vote_level);

        if (next_vote_level <= CNSS_BUS_WIDTH_LOW) {
            if (pHddCtx->hbw_requested) {
                hdd_remove_pm_qos();
                pHddCtx->hbw_requested = false;
            }
            if (vos_sched_handle_throughput_req(false))
                hddLog(LOGE, FL("low bandwidth set rx affinity fail"));
        } else {
            if (!pHddCtx->hbw_requested) {
                hdd_request_pm_qos(DISABLE_KRAIT_IDLE_PS_VAL);
                pHddCtx->hbw_requested = true;
            }
            if (vos_sched_handle_throughput_req(true))
                hddLog(LOGE, FL("high bandwidth set rx affinity fail"));
        }
    }

    /* fine-tuning parameters for RX Flows */
    temp_rx = (rx_packets + pHddCtx->prev_rx) / 2;
    pHddCtx->prev_rx = rx_packets;
    if (temp_rx > pHddCtx->cfg_ini->tcpDelackThresholdHigh)
        next_rx_level = WLAN_SVC_TP_HIGH;
    else
        next_rx_level = WLAN_SVC_TP_LOW;

    pHddCtx->hdd_txrx_hist[pHddCtx->hdd_txrx_hist_idx].next_rx_level
                                                          = next_rx_level;

    if (pHddCtx->cur_rx_level != next_rx_level) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_DEBUG,
               "%s: TCP DELACK trigger level %d, average_rx: %llu",
               __func__, next_rx_level, temp_rx);
        pHddCtx->cur_rx_level = next_rx_level;
        wlan_hdd_send_svc_nlink_msg(WLAN_SVC_WLAN_TP_IND,
                                    &next_rx_level,
                                    sizeof(next_rx_level));
    }

    /* fine-tuning parameters for TX Flows */
    temp_tx = (tx_packets + pHddCtx->prev_tx) / 2;
    pHddCtx->prev_tx = tx_packets;
    if (temp_tx > pHddCtx->cfg_ini->tcp_tx_high_tput_thres)
        next_tx_level = WLAN_SVC_TP_HIGH;
    else
        next_tx_level = WLAN_SVC_TP_LOW;

    if (pHddCtx->cur_tx_level != next_tx_level) {
        hddLog(VOS_TRACE_LEVEL_DEBUG,
               "%s: change TCP TX trigger level %d, average_tx: %llu ",
               __func__, next_tx_level, temp_tx);
        pHddCtx->cur_tx_level = next_tx_level;
        wlan_hdd_send_svc_nlink_msg(WLAN_SVC_WLAN_TP_TX_IND,
                                    &next_tx_level,
                                    sizeof(next_tx_level));
    }

    pHddCtx->hdd_txrx_hist[pHddCtx->hdd_txrx_hist_idx].next_tx_level
                                                          = next_tx_level;

    pHddCtx->hdd_txrx_hist_idx++;
    pHddCtx->hdd_txrx_hist_idx &= NUM_TX_RX_HISTOGRAM_MASK;
}

#define HDD_BW_GET_DIFF(x, y) ((x) >= (y) ? (x) - (y) : (ULONG_MAX - (y) + (x)))

static void hdd_bus_bw_compute_cbk(void *priv)
{
    hdd_context_t *pHddCtx = (hdd_context_t *)priv;
    hdd_adapter_t *pAdapter = NULL;
    uint64_t tx_packets= 0, rx_packets= 0;
    uint64_t total_tx = 0, total_rx = 0;
    hdd_adapter_list_node_t *pAdapterNode = NULL;
    VOS_STATUS status = 0;
    v_BOOL_t connected = FALSE;
#ifdef IPA_UC_OFFLOAD
    uint32_t ipa_tx_packets = 0, ipa_rx_packets = 0;
    hdd_adapter_t *pValidAdapter = NULL;
#endif /* IPA_UC_OFFLOAD */

    if (wlan_hdd_validate_context(pHddCtx))
        return;

    for (status = hdd_get_front_adapter(pHddCtx, &pAdapterNode);
            NULL != pAdapterNode && VOS_STATUS_SUCCESS == status;
            status = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pAdapterNode))
    {

        if ((pAdapter = pAdapterNode->pAdapter) == NULL)
            continue;

#ifdef IPA_UC_OFFLOAD
        if (NULL == pValidAdapter)
                pValidAdapter = pAdapter;
#endif /* IPA_UC_OFFLOAD */

        if ((pAdapter->device_mode == WLAN_HDD_INFRA_STATION ||
                    pAdapter->device_mode == WLAN_HDD_P2P_CLIENT) &&
                WLAN_HDD_GET_STATION_CTX_PTR(pAdapter)->conn_info.connState
                != eConnectionState_Associated) {

            continue;
        }

        if ((pAdapter->device_mode == WLAN_HDD_SOFTAP ||
                    pAdapter->device_mode == WLAN_HDD_P2P_GO) &&
                WLAN_HDD_GET_AP_CTX_PTR(pAdapter)->bApActive == VOS_FALSE) {

            continue;
        }

        tx_packets += HDD_BW_GET_DIFF(pAdapter->stats.tx_packets,
                pAdapter->prev_tx_packets);
        rx_packets += HDD_BW_GET_DIFF(pAdapter->stats.rx_packets,
                pAdapter->prev_rx_packets);

        total_rx += pAdapter->stats.rx_packets;
        total_tx += pAdapter->stats.tx_packets;


        spin_lock_bh(&pHddCtx->bus_bw_lock);
        pAdapter->prev_tx_packets = pAdapter->stats.tx_packets;
        pAdapter->prev_rx_packets = pAdapter->stats.rx_packets;
        spin_unlock_bh(&pHddCtx->bus_bw_lock);
        connected = TRUE;
    }

    pHddCtx->hdd_txrx_hist[pHddCtx->hdd_txrx_hist_idx].total_rx = total_rx;
    pHddCtx->hdd_txrx_hist[pHddCtx->hdd_txrx_hist_idx].total_tx = total_tx;
    pHddCtx->hdd_txrx_hist[pHddCtx->hdd_txrx_hist_idx].interval_rx = rx_packets;
    pHddCtx->hdd_txrx_hist[pHddCtx->hdd_txrx_hist_idx].interval_tx = tx_packets;

#ifdef IPA_UC_OFFLOAD
    hdd_ipa_uc_stat_query(pHddCtx, &ipa_tx_packets, &ipa_rx_packets);
    tx_packets += (uint64_t)ipa_tx_packets;
    rx_packets += (uint64_t)ipa_rx_packets;

    pValidAdapter->stats.tx_packets += ipa_tx_packets;
    pValidAdapter->stats.rx_packets += ipa_rx_packets;
#endif /* IPA_UC_OFFLOAD */
    if (!connected) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "bus bandwidth timer running in disconnected state");
        return;
    }

    hdd_cnss_request_bus_bandwidth(pHddCtx, tx_packets, rx_packets);

#ifdef IPA_OFFLOAD
    hdd_ipa_set_perf_level(pHddCtx, tx_packets, rx_packets);
#ifdef IPA_UC_OFFLOAD
    hdd_ipa_uc_stat_request(pValidAdapter, 2);
#endif /* IPA_UC_OFFLOAD */
#endif

    vos_timer_start(&pHddCtx->bus_bw_timer,
            pHddCtx->cfg_ini->busBandwidthComputeInterval);
}
#endif

/**
 * wlan_hdd_init_tx_rx_histogram() - init tx/rx histogram stats
 * @pHddCtx: hdd context
 *
 * Return: 0 for success
 */
int wlan_hdd_init_tx_rx_histogram(hdd_context_t *pHddCtx)
{
	pHddCtx->hdd_txrx_hist = vos_mem_malloc(
		 (sizeof(struct hdd_tx_rx_histogram) * NUM_TX_RX_HISTOGRAM));
	if (pHddCtx->hdd_txrx_hist == NULL) {
		hddLog(VOS_TRACE_LEVEL_FATAL,
			"%s: Failed malloc for hdd_txrx_hist",__func__);
		return -ENOMEM;
	}
	return 0;
}

/**
 * wlan_hdd_deinit_tx_rx_histogram() - deinit tx/rx histogram stats
 * @pHddCtx: hdd context
 *
 * Return: none
 */
void wlan_hdd_deinit_tx_rx_histogram(hdd_context_t *pHddCtx)
{
	if (pHddCtx->hdd_txrx_hist) {
		vos_mem_free(pHddCtx->hdd_txrx_hist);
		pHddCtx->hdd_txrx_hist = NULL;
	}
}

void wlan_hdd_display_tx_rx_histogram(hdd_context_t *pHddCtx)
{
    int i;

#ifdef MSM_PLATFORM
    hddLog(VOS_TRACE_LEVEL_ERROR, "BW Interval: %d curr_index %d",
                pHddCtx->cfg_ini->busBandwidthComputeInterval,
                pHddCtx->hdd_txrx_hist_idx);
    hddLog(VOS_TRACE_LEVEL_ERROR, "BW High TH: %d BW Med TH: %d BW Low TH: %d",
                pHddCtx->cfg_ini->busBandwidthHighThreshold,
                pHddCtx->cfg_ini->busBandwidthMediumThreshold,
                pHddCtx->cfg_ini->busBandwidthLowThreshold);
    hddLog(VOS_TRACE_LEVEL_ERROR, "TCP DEL High TH: %d TCP DEL Low TH: %d",
                pHddCtx->cfg_ini->tcpDelackThresholdHigh,
                pHddCtx->cfg_ini->tcpDelackThresholdLow);
#endif

    hddLog(VOS_TRACE_LEVEL_ERROR,
            "index, total_rx, interval_rx, total_tx, interval_tx, next_vote_level, next_rx_level, next_tx_level");
    for (i=0; i < NUM_TX_RX_HISTOGRAM; i++){
        hddLog(VOS_TRACE_LEVEL_ERROR,
               "%d: %llu, %llu, %llu, %llu, %d, %d, %d",
               i, pHddCtx->hdd_txrx_hist[i].total_rx,
               pHddCtx->hdd_txrx_hist[i].interval_rx,
               pHddCtx->hdd_txrx_hist[i].total_tx,
               pHddCtx->hdd_txrx_hist[i].interval_tx,
               pHddCtx->hdd_txrx_hist[i].next_vote_level,
               pHddCtx->hdd_txrx_hist[i].next_rx_level,
               pHddCtx->hdd_txrx_hist[i].next_tx_level);
    }
    return;
}

void wlan_hdd_clear_tx_rx_histogram(hdd_context_t *pHddCtx)
{
    pHddCtx->hdd_txrx_hist_idx = 0;
    vos_mem_zero(pHddCtx->hdd_txrx_hist,
        (sizeof(struct hdd_tx_rx_histogram) * NUM_TX_RX_HISTOGRAM));
}

/**---------------------------------------------------------------------------
  \brief hdd_11d_scan_done - callback to be executed when 11d scan is
                             completed to flush out the scan results

  11d scan is done during driver load and is a passive scan on all
  channels supported by the device, 11d scans may find some APs on
  frequencies which are forbidden to be used in the regulatory domain
  the device is operating in. If these APs are notified to the supplicant
  it may try to connect to these APs, thus flush out all the scan results
  which are present in SME after 11d scan is done.

  \return -  eHalStatus

  --------------------------------------------------------------------------*/
static eHalStatus hdd_11d_scan_done(tHalHandle halHandle, void *pContext,
                                    tANI_U8 sessionId, tANI_U32 scanId,
                                    eCsrScanStatus status)
{
    ENTER();

    sme_ScanFlushResult(halHandle, 0);

    EXIT();

    return eHAL_STATUS_SUCCESS;
}

#ifdef QCA_ARP_SPOOFING_WAR
int wlan_check_xxx(struct net_device *dev, int if_idex, void *data)
{
    hddLog(VOS_TRACE_LEVEL_INFO, "Checking for arp spoof packtes\n");
    return 0;
}

int hdd_filter_cb(tANI_U32 vdev_id, adf_nbuf_t skb, tANI_U32 type)
{
    hdd_adapter_t *pAdapter = NULL;
    hdd_context_t *pHddCtx = NULL;
    v_CONTEXT_t    pVosContext = NULL;
    int ret = 0;

    switch (type) {
        case RX_INTRA_BSS_FWD:
            pVosContext = vos_get_global_context(VOS_MODULE_ID_HDD, NULL);
            if(!pVosContext) {
               hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Global VOS context is Null", __func__);
               goto out;
            }

            pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext);
            if(!pHddCtx) {
               hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD context is Null",__func__);
               goto out;
            }

            pAdapter = hdd_get_adapter_by_vdev(pHddCtx, vdev_id);
            if (NULL == pAdapter) {
                hddLog(VOS_TRACE_LEVEL_FATAL,
                          "%s: vdev_id %d does not exist with host",
                          __func__, vdev_id);
                goto out;
            }

            if (*((unsigned short *)(skb->data + HDD_ARP_PACKET_TYPE_OFFSET))
                    == htons(ETH_P_ARP)) {

                ret = wlan_check_xxx(pAdapter->dev, pAdapter->dev->ifindex,
                        skb->data);
            }
            break;
        default:
            hddLog(VOS_TRACE_LEVEL_WARN, "Invalid filter type");
            goto out;
    }
out:
    return ret;
}
#endif
#if defined(CONFIG_HL_SUPPORT) && defined(QCA_BAD_PEER_TX_FLOW_CL)
/**
 * wlan_hdd_bad_peer_txctl() - HDD API to initialize the bad peer
 *                             tx flow control parameters
 * @pHddCtx:	HDD context which contains INI setting.
 *
 * Read configuation from INI setting, and then update the setting
 * of SME module.
 *
 * Return: NULL
 */
static void wlan_hdd_bad_peer_txctl(hdd_context_t *p_hdd_ctx)
{
	struct sme_bad_peer_txctl_param bad_peer_txctl;
	enum sme_max_bad_peer_thresh_levels level = IEEE80211_B_LEVEL;

	bad_peer_txctl.enabled        =
				p_hdd_ctx->cfg_ini->bad_peer_txctl_enable;
	bad_peer_txctl.period         =
				p_hdd_ctx->cfg_ini->bad_peer_txctl_prd;
	bad_peer_txctl.txq_limit      =
				p_hdd_ctx->cfg_ini->bad_peer_txctl_txq_lmt;
	bad_peer_txctl.tgt_backoff    =
				p_hdd_ctx->cfg_ini->bad_peer_tgt_backoff;
	bad_peer_txctl.tgt_report_prd =
				p_hdd_ctx->cfg_ini->bad_peer_tgt_report_prd;

	bad_peer_txctl.thresh[level].cond       =
				p_hdd_ctx->cfg_ini->bad_peer_cond_ieee80211b;
	bad_peer_txctl.thresh[level].delta      =
				p_hdd_ctx->cfg_ini->bad_peer_delta_ieee80211b;
	bad_peer_txctl.thresh[level].percentage =
				p_hdd_ctx->cfg_ini->bad_peer_pct_ieee80211b;
	bad_peer_txctl.thresh[level].thresh     =
				p_hdd_ctx->cfg_ini->bad_peer_tput_ieee80211b;
	bad_peer_txctl.thresh[level].limit      =
				p_hdd_ctx->cfg_ini->bad_peer_limit_ieee80211b;

	level++;
	bad_peer_txctl.thresh[level].cond       =
				p_hdd_ctx->cfg_ini->bad_peer_cond_ieee80211ag;
	bad_peer_txctl.thresh[level].delta      =
				p_hdd_ctx->cfg_ini->bad_peer_delta_ieee80211ag;
	bad_peer_txctl.thresh[level].percentage =
				p_hdd_ctx->cfg_ini->bad_peer_pct_ieee80211ag;
	bad_peer_txctl.thresh[level].thresh     =
				p_hdd_ctx->cfg_ini->bad_peer_tput_ieee80211ag;
	bad_peer_txctl.thresh[level].limit      =
				p_hdd_ctx->cfg_ini->bad_peer_limit_ieee80211ag;

	level++;
	bad_peer_txctl.thresh[level].cond       =
				p_hdd_ctx->cfg_ini->bad_peer_cond_ieee80211n;
	bad_peer_txctl.thresh[level].delta      =
				p_hdd_ctx->cfg_ini->bad_peer_delta_ieee80211n;
	bad_peer_txctl.thresh[level].percentage =
				p_hdd_ctx->cfg_ini->bad_peer_pct_ieee80211n;
	bad_peer_txctl.thresh[level].thresh     =
				p_hdd_ctx->cfg_ini->bad_peer_tput_ieee80211n;
	bad_peer_txctl.thresh[level].limit      =
				p_hdd_ctx->cfg_ini->bad_peer_limit_ieee80211n;

	level++;
	bad_peer_txctl.thresh[level].cond       =
				p_hdd_ctx->cfg_ini->bad_peer_cond_ieee80211ag;
	bad_peer_txctl.thresh[level].delta      =
				p_hdd_ctx->cfg_ini->bad_peer_delta_ieee80211ac;
	bad_peer_txctl.thresh[level].percentage =
				p_hdd_ctx->cfg_ini->bad_peer_pct_ieee80211ac;
	bad_peer_txctl.thresh[level].thresh     =
				p_hdd_ctx->cfg_ini->bad_peer_tput_ieee80211ac;
	bad_peer_txctl.thresh[level].limit      =
				p_hdd_ctx->cfg_ini->bad_peer_limit_ieee80211ac;

	if (eHAL_STATUS_SUCCESS !=
		sme_init_bad_peer_txctl_info(p_hdd_ctx->hHal, bad_peer_txctl)) {
		hddLog(VOS_TRACE_LEVEL_ERROR,
			"%s: Error while initializing bad peer Txctl infor",
			__func__);
	}
}
#else
static inline void wlan_hdd_bad_peer_txctl(hdd_context_t *p_hdd_ctx)
{
	/* no-op */
}
#endif /* defined(CONFIG_HL_SUPPORT) && defined(QCA_BAD_PEER_TX_FLOW_CL) */

#ifdef WLAN_FEATURE_OFFLOAD_PACKETS
/**
 * hdd_init_offloaded_packets_ctx() - Initialize offload packets context
 * @hdd_ctx: hdd global context
 *
 * Return: none
 */
static void hdd_init_offloaded_packets_ctx(hdd_context_t *hdd_ctx)
{
	uint8_t i;

	mutex_init(&hdd_ctx->op_ctx.op_lock);
	for (i = 0; i < MAXNUM_PERIODIC_TX_PTRNS; i++) {
		hdd_ctx->op_ctx.op_table[i].request_id = MAX_REQUEST_ID;
		hdd_ctx->op_ctx.op_table[i].pattern_id = i;
	}
}
#else
static void hdd_init_offloaded_packets_ctx(hdd_context_t *hdd_ctx)
{
}
#endif

#ifdef WLAN_FEATURE_WOW_PULSE
/**
* wlan_hdd_set_wow_pulse() - call SME to send wmi cmd of wow pulse
* @phddctx: hdd_context_t structure pointer
* @enable: enable or disable this behaviour
*
* Return: int
*/
static int wlan_hdd_set_wow_pulse(hdd_context_t *phddctx, bool enable)
{
	hdd_config_t *pcfg_ini = phddctx->cfg_ini;
	struct wow_pulse_mode wow_pulse_set_info;
	VOS_STATUS status;

	hddLog(LOG1, FL("wow pulse enable flag is %d"), enable);

	if (false == phddctx->cfg_ini->wow_pulse_support)
		return 0;

	/* prepare the request to send to SME */
	if (enable == true) {
		wow_pulse_set_info.wow_pulse_enable = true;
		wow_pulse_set_info.wow_pulse_pin =
				pcfg_ini->wow_pulse_pin;
		wow_pulse_set_info.wow_pulse_interval_low =
				pcfg_ini->wow_pulse_interval_low;
		wow_pulse_set_info.wow_pulse_interval_high=
				pcfg_ini->wow_pulse_interval_high;
	} else {
		wow_pulse_set_info.wow_pulse_enable = false;
		wow_pulse_set_info.wow_pulse_pin = 0;
		wow_pulse_set_info.wow_pulse_interval_low = 0;
		wow_pulse_set_info.wow_pulse_interval_high= 0;
	}
	hddLog(LOG1,"%s: enable %d pin %d low %d high %d",
		__func__, wow_pulse_set_info.wow_pulse_enable,
		wow_pulse_set_info.wow_pulse_pin,
		wow_pulse_set_info.wow_pulse_interval_low,
		wow_pulse_set_info.wow_pulse_interval_high);

	status = sme_set_wow_pulse(&wow_pulse_set_info);
	if (VOS_STATUS_E_FAILURE == status) {
		hddLog(LOGE,
			"%s: sme_set_wow_pulse failure!", __func__);
		return -EIO;
	}
	hddLog(LOG2,
		"%s: sme_set_wow_pulse success!", __func__);
	return 0;
}
#else
static int inline wlan_hdd_set_wow_pulse(hdd_context_t *phddctx, bool enable)
{
	return 0;
}
#endif

/**
 * hdd_tsf_init() - Initialize the TSF synchronization interface
 * @hdd_ctx: HDD global context
 *
 * When TSF synchronization via GPIO is supported by the driver and
 * has been enabled in the configuration file, this function plumbs
 * the GPIO value down to firmware via SME.
 *
 * Return: None
 */
#ifdef WLAN_FEATURE_TSF
static void hdd_tsf_init(hdd_context_t *hdd_ctx)
{
	eHalStatus hal_status;

	if (hdd_ctx->cfg_ini->tsf_gpio_pin == TSF_GPIO_PIN_INVALID)
		return;

	hal_status = sme_set_tsf_gpio(hdd_ctx->hHal,
				      hdd_ctx->cfg_ini->tsf_gpio_pin);
	if (eHAL_STATUS_SUCCESS != hal_status)
		hddLog(LOGE, FL("set tsf GPIO failed, status: %d"),
		       hal_status);
}
#else
static void hdd_tsf_init(hdd_context_t *hdd_ctx)
{
}
#endif

/**---------------------------------------------------------------------------

  \brief hdd_wlan_startup() - HDD init function

  This is the driver startup code executed once a WLAN device has been detected

  \param  - dev - Pointer to the underlying device

  \return -  0 for success, < 0 for failure

  --------------------------------------------------------------------------*/

int hdd_wlan_startup(struct device *dev, v_VOID_t *hif_sc)
{
   VOS_STATUS status;
   hdd_adapter_t *pAdapter = NULL;
#ifdef WLAN_OPEN_P2P_INTERFACE
   hdd_adapter_t *pP2pAdapter = NULL;
#endif
   hdd_context_t *pHddCtx = NULL;
   v_CONTEXT_t pVosContext= NULL;
   eHalStatus hal_status;
   int ret;
   int i;
   struct wiphy *wiphy;
   unsigned long rc;
   tSmeThermalParams thermalParam;
   tSirTxPowerLimit *hddtxlimit;
#ifdef FEATURE_WLAN_CH_AVOID
#ifdef CONFIG_CNSS
   uint16_t unsafe_channel_count;
   int unsafeChannelIndex;
#endif
#endif
   tANI_U8 rtnl_lock_enable;
   tANI_U8 reg_netdev_notifier_done = FALSE;
   hdd_adapter_t *dot11_adapter = NULL;
#ifdef QCA_ARP_SPOOFING_WAR
   adf_os_device_t adf_ctx;
#endif

   ENTER();

   if (WLAN_IS_EPPING_ENABLED(con_mode)) {
       /* if epping enabled redirect start to epping module */
      ret = epping_wlan_startup(dev, hif_sc);
      EXIT();
      return ret;
   }
   /*
    * cfg80211: wiphy allocation
    */
   wiphy = wlan_hdd_cfg80211_wiphy_alloc(sizeof(hdd_context_t)) ;

   if(wiphy == NULL)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: cfg80211 init failed", __func__);
      return -EIO;
   }

   pHddCtx = wiphy_priv(wiphy);

   //Initialize the adapter context to zeros.
   vos_mem_zero(pHddCtx, sizeof( hdd_context_t ));

   pHddCtx->wiphy = wiphy;
   pHddCtx->isLoadInProgress = TRUE;
   pHddCtx->ioctl_scan_mode = eSIR_ACTIVE_SCAN;
   vos_set_wakelock_logging(false);

   vos_set_load_unload_in_progress(VOS_MODULE_ID_VOSS, TRUE);
   vos_set_load_in_progress(VOS_MODULE_ID_VOSS, TRUE);

   /*Get vos context here bcoz vos_open requires it*/
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

   if(pVosContext == NULL)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Failed vos_get_global_context",__func__);
      goto err_free_hdd_context;
   }
#ifdef QCA_ARP_SPOOFING_WAR
   adf_ctx = vos_get_context(VOS_MODULE_ID_ADF, pVosContext);
   if (adf_ctx == NULL) {
       hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Failed vos_get_global_context",__func__);
       goto err_free_hdd_context;
   }
   adf_ctx->filter_cb = (void *)hdd_filter_cb;
#endif

   //Save the Global VOSS context in adapter context for future.
   pHddCtx->pvosContext = pVosContext;

   //Save the adapter context in global context for future.
   ((VosContextType*)(pVosContext))->pHDDContext = (v_VOID_t*)pHddCtx;

   pHddCtx->parent_dev = dev;

   init_completion(&pHddCtx->full_pwr_comp_var);
   init_completion(&pHddCtx->standby_comp_var);
   init_completion(&pHddCtx->req_bmps_comp_var);
   hdd_init_bpf_completion();
#ifdef FEATURE_WLAN_EXTSCAN
   init_completion(&pHddCtx->ext_scan_context.response_event);
#endif /* FEATURE_WLAN_EXTSCAN */

   hdd_init_ll_stats_ctx(pHddCtx);

   spin_lock_init(&pHddCtx->schedScan_lock);

   hdd_list_init( &pHddCtx->hddAdapters, MAX_NUMBER_OF_ADAPTERS );

#ifdef FEATURE_WLAN_TDLS
   /* tdls_lock is initialized before an hdd_open_adapter ( which is
    * invoked by other instances also) to protect the concurrent
    * access for the Adapters by TDLS module.
    */
   mutex_init(&pHddCtx->tdls_lock);
#endif

   status = wlan_hdd_init_tx_rx_histogram(pHddCtx);
   if (status != 0) {
       goto err_free_hdd_context;
   }

   spin_lock_init(&pHddCtx->dfs_lock);
   hdd_init_offloaded_packets_ctx(pHddCtx);
   // Load all config first as TL config is needed during vos_open
   pHddCtx->cfg_ini = (hdd_config_t*) kmalloc(sizeof(hdd_config_t), GFP_KERNEL);
   if(pHddCtx->cfg_ini == NULL)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Failed kmalloc hdd_config_t",__func__);
      goto err_histogram;
   }

   vos_mem_zero(pHddCtx->cfg_ini, sizeof( hdd_config_t ));

   // Read and parse the qcom_cfg.ini file
   status = hdd_parse_config_ini( pHddCtx );
   if ( VOS_STATUS_SUCCESS != status )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: error parsing %s",
             __func__, WLAN_INI_FILE);
      goto err_config;
   }

   ((VosContextType*)pVosContext)->pHIFContext = hif_sc;

   /* store target type and target version info in hdd ctx */
   pHddCtx->target_type = ((struct ol_softc *)hif_sc)->target_type;

   pHddCtx->current_intf_count=0;
   pHddCtx->max_intf_count = CSR_ROAM_SESSION_MAX;

   /* INI has been read, initialise the configuredMcastBcastFilter with
    * INI value as this will serve as the default value
    */
   pHddCtx->configuredMcastBcastFilter = pHddCtx->cfg_ini->mcastBcastFilterSetting;
   hddLog(VOS_TRACE_LEVEL_INFO, "Setting configuredMcastBcastFilter: %d",
                   pHddCtx->cfg_ini->mcastBcastFilterSetting);

   if (false == hdd_is_5g_supported(pHddCtx))
   {
      //5Ghz is not supported.
      if (1 != pHddCtx->cfg_ini->nBandCapability)
      {
         hddLog(VOS_TRACE_LEVEL_INFO,
                "%s: Setting pHddCtx->cfg_ini->nBandCapability = 1", __func__);
         pHddCtx->cfg_ini->nBandCapability = 1;
      }
   }

   if (pHddCtx->cfg_ini->fhostNSOffload)
       pHddCtx->ns_offload_enable = true;

   /*
    * If SNR Monitoring is enabled, FW has to parse all beacons
    * for calculating and storing the average SNR, so set Nth beacon
    * filter to 1 to enable FW to parse all the beacons
    */
   if (1 == pHddCtx->cfg_ini->fEnableSNRMonitoring)
   {
      /* The log level is deliberately set to WARN as overriding
       * nthBeaconFilter to 1 will increase power consumption and this
       * might just prove helpful to detect the power issue.
       */
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: Setting pHddCtx->cfg_ini->nthBeaconFilter = 1", __func__);
      pHddCtx->cfg_ini->nthBeaconFilter = 1;
   }
   /*
    * cfg80211: Initialization  ...
    */
   if (0 < wlan_hdd_cfg80211_init(dev, wiphy, pHddCtx->cfg_ini)) {
          hddLog(LOGE,
                 "%s: wlan_hdd_cfg80211_init return failure", __func__);
          goto err_config;
   }

   /* Initialize struct for saving f/w log setting will be used
   after ssr */
   pHddCtx->fw_log_settings.enable = pHddCtx->cfg_ini->enablefwlog;
   pHddCtx->fw_log_settings.dl_type = 0;
   pHddCtx->fw_log_settings.dl_report = 0;
   pHddCtx->fw_log_settings.dl_loglevel = 0;
   pHddCtx->fw_log_settings.index = 0;
   for (i = 0; i < MAX_MOD_LOGLEVEL; i++) {
       pHddCtx->fw_log_settings.dl_mod_loglevel[i] = 0;
   }

   if (VOS_FTM_MODE != hdd_get_conparam()) {
       vos_set_multicast_logging(pHddCtx->cfg_ini->multicast_host_fw_msgs);

       if (wlan_hdd_logging_sock_activate_svc(pHddCtx) < 0)
           goto err_config;

       /*
        * Update VOS trace levels based upon the code. The multicast log
        * log levels of the code need not be set when the logger thread
        * is not enabled.
        */
       if (vos_is_multicast_logging())
           wlan_logging_set_log_level();
   }

   /*
    * Update VOS trace levels based upon the cfg.ini
    */
   hdd_vos_trace_enable(VOS_MODULE_ID_TL,
                        pHddCtx->cfg_ini->vosTraceEnableTL);
   hdd_vos_trace_enable(VOS_MODULE_ID_WDI,
                        pHddCtx->cfg_ini->vosTraceEnableWDI);
   hdd_vos_trace_enable(VOS_MODULE_ID_HDD,
                        pHddCtx->cfg_ini->vosTraceEnableHDD);
   hdd_vos_trace_enable(VOS_MODULE_ID_SME,
                        pHddCtx->cfg_ini->vosTraceEnableSME);
   hdd_vos_trace_enable(VOS_MODULE_ID_PE,
                        pHddCtx->cfg_ini->vosTraceEnablePE);
   hdd_vos_trace_enable(VOS_MODULE_ID_PMC,
                         pHddCtx->cfg_ini->vosTraceEnablePMC);
   hdd_vos_trace_enable(VOS_MODULE_ID_WDA,
                        pHddCtx->cfg_ini->vosTraceEnableWDA);
   hdd_vos_trace_enable(VOS_MODULE_ID_SYS,
                        pHddCtx->cfg_ini->vosTraceEnableSYS);
   hdd_vos_trace_enable(VOS_MODULE_ID_VOSS,
                        pHddCtx->cfg_ini->vosTraceEnableVOSS);
   hdd_vos_trace_enable(VOS_MODULE_ID_SAP,
                        pHddCtx->cfg_ini->vosTraceEnableSAP);
   hdd_vos_trace_enable(VOS_MODULE_ID_HDD_SOFTAP,
                        pHddCtx->cfg_ini->vosTraceEnableHDDSAP);
   hdd_vos_trace_enable(VOS_MODULE_ID_HDD_DATA,
                        pHddCtx->cfg_ini->vosTraceEnableHDDDATA);
   hdd_vos_trace_enable(VOS_MODULE_ID_HDD_SAP_DATA,
                        pHddCtx->cfg_ini->vosTraceEnableHDDSAPDATA);
   hdd_vos_trace_enable(VOS_MODULE_ID_HIF,
                        pHddCtx->cfg_ini->vosTraceEnableHIF);
   hdd_vos_trace_enable(VOS_MODULE_ID_TXRX,
                        pHddCtx->cfg_ini->vosTraceEnableTXRX);
   hdd_vos_trace_enable(VOS_MODULE_ID_HTC,
                        pHddCtx->cfg_ini->vosTraceEnableHTC);
   hdd_vos_trace_enable(VOS_MODULE_ID_ADF,
                        pHddCtx->cfg_ini->vosTraceEnableADF);
   hdd_vos_trace_enable(VOS_MODULE_ID_CFG,
                        pHddCtx->cfg_ini->vosTraceEnableCFG);

   print_hdd_cfg(pHddCtx);

   if (VOS_FTM_MODE == hdd_get_conparam())
       goto ftm_processing;

   //Open watchdog module
   if(pHddCtx->cfg_ini->fIsLogpEnabled)
   {
      status = vos_watchdog_open(pVosContext,
         &((VosContextType*)pVosContext)->vosWatchdog, sizeof(VosWatchdogContext));

      if(!VOS_IS_STATUS_SUCCESS( status ))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_watchdog_open failed",__func__);
         goto err_wdclose;
      }
   }

   pHddCtx->isLogpInProgress = FALSE;
   vos_set_logp_in_progress(VOS_MODULE_ID_VOSS, FALSE);

   status = vos_nv_open();
   if (!VOS_IS_STATUS_SUCCESS(status))
   {
      /* NV module cannot be initialized */
      hddLog( VOS_TRACE_LEVEL_FATAL,
            "%s: vos_nv_open failed", __func__);
      goto err_wdclose;
   }

   hdd_wlan_green_ap_init(pHddCtx);

   status = vos_open( &pVosContext, 0);
   if ( !VOS_IS_STATUS_SUCCESS( status ))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: vos_open failed", __func__);
      goto err_vos_nv_close;
   }

#if      !defined(REMOVE_PKT_LOG)
   hif_init_pdev_txrx_handle(hif_sc,
                             vos_get_context(VOS_MODULE_ID_TXRX, pVosContext));
#endif

   pHddCtx->hHal = (tHalHandle)vos_get_context( VOS_MODULE_ID_SME, pVosContext );

   if ( NULL == pHddCtx->hHal )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: HAL context is null", __func__);
      goto err_vosclose;
   }

   status = vos_preStart( pHddCtx->pvosContext );
   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: vos_preStart failed", __func__);
      goto err_vosclose;
   }

   wlan_hdd_update_wiphy(wiphy, pHddCtx->cfg_ini);

   if (sme_IsFeatureSupportedByFW(DOT11AC)) {
      hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: support 11ac", __func__);
   } else {
      hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: not support 11ac", __func__);
      if ((pHddCtx->cfg_ini->dot11Mode == eHDD_DOT11_MODE_11ac_ONLY)||
          (pHddCtx->cfg_ini->dot11Mode == eHDD_DOT11_MODE_11ac)) {

          pHddCtx->cfg_ini->dot11Mode = eHDD_DOT11_MODE_11n;
          pHddCtx->cfg_ini->sap_p2p_11ac_override = 0;
      }
   }

   status = wlan_hdd_reg_init(pHddCtx);
   if (status != VOS_STATUS_SUCCESS) {
      hddLog(VOS_TRACE_LEVEL_FATAL,
             "%s: Failed to init channel list", __func__);
      goto err_vosclose;
   }

   if (0 != wlan_hdd_set_wow_pulse(pHddCtx, true)) {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Failed to set wow pulse", __func__);
   }


   /* Set 802.11p config
    * TODO-OCB: This has been temporarily added here to ensure this paramter
    * is set in CSR when we init the channel list. This should be removed
    * once the 5.9 GHz channels are added to the regulatory domain.
    */
   hdd_set_dot11p_config(pHddCtx);

   if (0 == enable_dfs_chan_scan || 1 == enable_dfs_chan_scan)
   {
      pHddCtx->cfg_ini->enableDFSChnlScan = enable_dfs_chan_scan;
      hddLog(VOS_TRACE_LEVEL_INFO, "%s: module enable_dfs_chan_scan set to %d",
             __func__, enable_dfs_chan_scan);
   }
   if (0 == enable_11d || 1 == enable_11d)
   {
      pHddCtx->cfg_ini->Is11dSupportEnabled = enable_11d;
      hddLog(VOS_TRACE_LEVEL_INFO, "%s: module enable_11d set to %d",
             __func__, enable_11d);
   }

   /* Note that the vos_preStart() sequence triggers the cfg download.
      The cfg download must occur before we update the SME config
      since the SME config operation must access the cfg database */
   status = hdd_set_sme_config( pHddCtx );

   if ( VOS_STATUS_SUCCESS != status )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Failed hdd_set_sme_config", __func__);
      goto err_wiphy_unregister;
   }

   ret = process_wma_set_command(0, WMI_PDEV_PARAM_TX_CHAIN_MASK_1SS,
                                 pHddCtx->cfg_ini->tx_chain_mask_1ss,
                                 PDEV_CMD);
   if (0 != ret) {
       hddLog(VOS_TRACE_LEVEL_ERROR,
              "%s: WMI_PDEV_PARAM_TX_CHAIN_MASK_1SS failed %d",
              __func__, ret);
   }

   status = hdd_set_sme_chan_list(pHddCtx);
   if (status != VOS_STATUS_SUCCESS) {
      hddLog(VOS_TRACE_LEVEL_FATAL,
             "%s: Failed to init channel list", __func__);
      goto err_wiphy_unregister;
   }

   /* In the integrated architecture we update the configuration from
      the INI file and from NV before vOSS has been started so that
      the final contents are available to send down to the cCPU   */

   // Apply the cfg.ini to cfg.dat
   if (FALSE == hdd_update_config_dat(pHddCtx))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: config update failed",__func__ );
      goto err_wiphy_unregister;
   }

   if ( VOS_STATUS_SUCCESS != hdd_update_mac_config( pHddCtx ) )
   {
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: can't update mac config, using MAC from ini file",
             __func__);
   }

   {
      eHalStatus halStatus;
      /* Set the MAC Address Currently this is used by HAL to
       * add self sta. Remove this once self sta is added as
       * part of session open.
       */
      halStatus = cfgSetStr( pHddCtx->hHal, WNI_CFG_STA_ID,
                             (v_U8_t *)&pHddCtx->cfg_ini->intfMacAddr[0],
                             sizeof( pHddCtx->cfg_ini->intfMacAddr[0]) );

      if (!HAL_STATUS_SUCCESS( halStatus ))
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Failed to set MAC Address. "
                "HALStatus is %08d [x%08x]",__func__, halStatus, halStatus );
         goto err_wiphy_unregister;
      }
   }

#ifdef IPA_OFFLOAD
#ifdef SYNC_IPA_READY
   /* Check if IPA is ready before calling any IPA API */
   if ((ret = ipa_register_ipa_ready_cb((void *)hdd_ipa_ready_cb,
                                      (void *)pHddCtx)) == -EEXIST) {
      hddLog(VOS_TRACE_LEVEL_FATAL, FL("IPA is ready"));
   } else if (ret >= 0) {
      hddLog(VOS_TRACE_LEVEL_FATAL,
             FL("IPA is not ready - wait until it is ready"));
      init_completion(&pHddCtx->ipa_ready);
      wait_for_completion(&pHddCtx->ipa_ready);
   } else {
      hddLog(VOS_TRACE_LEVEL_FATAL,
             FL("IPA is not ready - Fail to register IPA ready callback"));
      goto err_wiphy_unregister;
   }
#endif
   if (hdd_ipa_init(pHddCtx) == VOS_STATUS_E_FAILURE)
      goto err_wiphy_unregister;
#endif

   /*Start VOSS which starts up the SME/MAC/HAL modules and everything else */
   status = vos_start( pHddCtx->pvosContext );
   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_start failed",__func__);
#ifdef IPA_OFFLOAD
      goto err_ipa_cleanup;
#else
      goto err_wiphy_unregister;
#endif
   }

#ifdef FEATURE_WLAN_CH_AVOID
#ifdef CONFIG_CNSS
   vos_get_wlan_unsafe_channel(pHddCtx->unsafe_channel_list,
                                &(pHddCtx->unsafe_channel_count),
                                sizeof(v_U16_t) * NUM_20MHZ_RF_CHANNELS);
   hddLog(LOG1,"%s: num of unsafe channels is %d. ",
                __func__, pHddCtx->unsafe_channel_count);

   unsafe_channel_count = VOS_MIN((uint16_t)pHddCtx->unsafe_channel_count,
                              (uint16_t)NUM_20MHZ_RF_CHANNELS);

   for (unsafeChannelIndex = 0;
       unsafeChannelIndex < unsafe_channel_count;
       unsafeChannelIndex++) {
       hddLog(LOG1,"%s: channel %d is not safe. ",
         __func__, pHddCtx->unsafe_channel_list[unsafeChannelIndex]);
   }

   /* Plug in avoid channel notification callback */
   sme_AddChAvoidCallback(pHddCtx->hHal,
                          hdd_ch_avoid_cb);
#endif
#endif /* FEATURE_WLAN_CH_AVOID */

   status = hdd_post_voss_start_config( pHddCtx );
   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hdd_post_voss_start_config failed",
         __func__);
      goto err_vosstop;
   }

#ifdef QCA_PKT_PROTO_TRACE
   vos_pkt_proto_trace_init();
#endif /* QCA_PKT_PROTO_TRACE */

 ftm_processing:
   if (VOS_FTM_MODE == hdd_get_conparam())
   {
      if ( VOS_STATUS_SUCCESS != wlan_hdd_ftm_open(pHddCtx) )
      {
          hddLog(VOS_TRACE_LEVEL_FATAL,"%s: wlan_hdd_ftm_open Failed",__func__);
          goto err_config;
      }
#if  defined(QCA_WIFI_FTM)
      if (hdd_ftm_start(pHddCtx))
      {
          hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hdd_ftm_start Failed",__func__);
          goto err_free_ftm_open;
      }
#endif

      /* registration of wiphy dev with cfg80211 */
      if (0 > wlan_hdd_cfg80211_register(wiphy)) {
          hddLog(LOGE, FL("wiphy register failed"));
          goto err_free_ftm_open;
      }

      vos_set_load_unload_in_progress(VOS_MODULE_ID_VOSS, FALSE);
      vos_set_load_in_progress(VOS_MODULE_ID_VOSS, FALSE);
      pHddCtx->isLoadInProgress = FALSE;

      memdump_init();
      hddLog(LOGE, FL("FTM driver loaded"));
      complete(&wlan_start_comp);
      return VOS_STATUS_SUCCESS;
   }

#if defined(CONFIG_HDD_INIT_WITH_RTNL_LOCK)
   rtnl_lock();
   rtnl_lock_enable = TRUE;
#else
   rtnl_lock_enable = FALSE;
#endif

   if (pHddCtx->cfg_ini->dot11p_mode == WLAN_HDD_11P_STANDALONE) {
       /* Create only 802.11p interface */
      pAdapter = hdd_open_adapter(pHddCtx, WLAN_HDD_OCB,
          "wlanocb%d", wlan_hdd_get_intf_addr(pHddCtx), rtnl_lock_enable);
   } else {
      pAdapter = hdd_open_adapter( pHddCtx, WLAN_HDD_INFRA_STATION, "wlan%d",
          wlan_hdd_get_intf_addr(pHddCtx), rtnl_lock_enable );

#ifdef WLAN_OPEN_P2P_INTERFACE
      /* Open P2P device interface */
      if (pAdapter != NULL) {
         if (pHddCtx->cfg_ini->isP2pDeviceAddrAdministrated &&
             !(pHddCtx->cfg_ini->intfMacAddr[0].bytes[0] & 0x02)) {
            vos_mem_copy(pHddCtx->p2pDeviceAddress.bytes,
                         pHddCtx->cfg_ini->intfMacAddr[0].bytes,
                         sizeof(tSirMacAddr));

            /* Generate the P2P Device Address.  This consists of the device's
             * primary MAC address with the locally administered bit set.
             */
            pHddCtx->p2pDeviceAddress.bytes[0] |= 0x02;
         } else {
            uint8_t* p2p_dev_addr = wlan_hdd_get_intf_addr(pHddCtx);
            if (p2p_dev_addr != NULL) {
               vos_mem_copy(&pHddCtx->p2pDeviceAddress.bytes[0],
                            p2p_dev_addr, VOS_MAC_ADDR_SIZE);
            } else {
               hddLog(VOS_TRACE_LEVEL_FATAL,
                   FL("Failed to allocate mac_address for p2p_device"));
               goto err_close_adapter;
            }
         }

         pP2pAdapter = hdd_open_adapter(pHddCtx, WLAN_HDD_P2P_DEVICE, "p2p%d",
                           &pHddCtx->p2pDeviceAddress.bytes[0],
                           rtnl_lock_enable);

         if (NULL == pP2pAdapter) {
            hddLog(VOS_TRACE_LEVEL_FATAL,
                FL("Failed to do hdd_open_adapter for P2P Device Interface"));
            goto err_close_adapter;
         }
      }
#endif /* WLAN_OPEN_P2P_INTERFACE */

      /* Open 802.11p Interface */
      if (pAdapter != NULL) {
         if (pHddCtx->cfg_ini->dot11p_mode == WLAN_HDD_11P_CONCURRENT) {
            dot11_adapter = hdd_open_adapter(pHddCtx, WLAN_HDD_OCB,
                       "wlanocb%d", wlan_hdd_get_intf_addr(pHddCtx),
                       rtnl_lock_enable);

            if (dot11_adapter == NULL) {
               hddLog(VOS_TRACE_LEVEL_FATAL,
                   FL("hdd_open_adapter() failed for 802.11p Interface"));
               goto err_close_adapter;
            }
         }
      }
   }

   if( pAdapter == NULL )
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: hdd_open_adapter failed", __func__);
      goto err_close_adapter;
   }


   /* Target hw version/revision would only be retrieved after
      firmware download */
   hif_get_hw_info(hif_sc, &pHddCtx->target_hw_version,
                   &pHddCtx->target_hw_revision);

   /* Get the wlan hw/fw version */
   hdd_wlan_get_version(pAdapter, NULL, NULL);

   /* pass target_fw_version to HIF layer */
   hif_set_fw_info(hif_sc, pHddCtx->target_fw_version);

   if (country_code)
   {
      eHalStatus ret;

      INIT_COMPLETION(pAdapter->change_country_code);
      hdd_checkandupdate_dfssetting(pAdapter, country_code);

      ret = sme_ChangeCountryCode(pHddCtx->hHal,
            (void *)(tSmeChangeCountryCallback)
            wlan_hdd_change_country_code_callback,
            country_code, pAdapter, pHddCtx->pvosContext, eSIR_TRUE, eSIR_TRUE);
      if (eHAL_STATUS_SUCCESS == ret)
      {
          rc = wait_for_completion_timeout(
                &pAdapter->change_country_code,
                msecs_to_jiffies(WLAN_WAIT_TIME_COUNTRY));
          if (!rc) {
              hddLog(VOS_TRACE_LEVEL_ERROR,
                   "%s: SME while setting country code timed out", __func__);
          }
      }
      else
      {
          hddLog(VOS_TRACE_LEVEL_ERROR,
                 "%s: SME Change Country code from module param fail ret=%d", __func__, ret);
          ret = -EINVAL;
      }
   }


#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
   if(!(IS_ROAM_SCAN_OFFLOAD_FEATURE_ENABLE))
   {
      hddLog(VOS_TRACE_LEVEL_DEBUG,"%s: ROAM_SCAN_OFFLOAD Feature not supported",__func__);
      pHddCtx->cfg_ini->isRoamOffloadScanEnabled = 0;
      sme_UpdateRoamScanOffloadEnabled((tHalHandle)(pHddCtx->hHal),
                       pHddCtx->cfg_ini->isRoamOffloadScanEnabled);
   }
#endif
#ifdef FEATURE_WLAN_SCAN_PNO
   /*SME must send channel update configuration to FW*/
   sme_UpdateChannelConfig(pHddCtx->hHal);
#endif
   sme_Register11dScanDoneCallback(pHddCtx->hHal, hdd_11d_scan_done);

   /* Fwr capabilities received, Set the Dot11 mode */
   sme_SetDefDot11Mode(pHddCtx->hHal);

   wlansap_set_tx_leakage_threshold(pHddCtx->hHal,
        pHddCtx->cfg_ini->sap_tx_leakage_threshold);

   /* Register with platform driver as client for Suspend/Resume */
   status = hddRegisterPmOps(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hddRegisterPmOps failed",__func__);
      goto err_close_adapter;
   }

   /* Open debugfs interface */
   if (VOS_STATUS_SUCCESS != hdd_debugfs_init(pAdapter))
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "%s: hdd_debugfs_init failed!", __func__);
   }

   /* Register TM level change handler function to the platform */
   status = hddDevTmRegisterNotifyCallback(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hddDevTmRegisterNotifyCallback failed",__func__);
      goto err_unregister_pmops;
   }

   /* register for riva power on lock to platform driver */
   if (req_riva_power_on_lock("wlan"))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: req riva power on lock failed",
                                     __func__);
      goto err_unregister_pmops;
   }
#if !defined(CONFIG_HDD_INIT_WITH_RTNL_LOCK)
   // register net device notifier for device change notification
   ret = register_netdevice_notifier(&hdd_netdev_notifier);

   if(ret < 0)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: register_netdevice_notifier failed",__func__);
      goto err_free_power_on_lock;
   }
   reg_netdev_notifier_done = TRUE;
#endif

   //Initialize the nlink service
   if(nl_srv_init() != 0)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: nl_srv_init failed", __func__);
      goto err_reg_netdev;
   }

#ifdef WLAN_KD_READY_NOTIFIER
   pHddCtx->kd_nl_init = 1;
#endif /* WLAN_KD_READY_NOTIFIER */

   //Initialize the BTC service
   if(btc_activate_service(pHddCtx) != 0)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: btc_activate_service failed",__func__);
      goto err_nl_srv;
   }

#ifdef FEATURE_OEM_DATA_SUPPORT
   //Initialize the OEM service
   if (oem_activate_service(pHddCtx) != 0)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,
             "%s: oem_activate_service failed", __func__);
      goto err_nl_srv;
   }
#endif

#ifdef PTT_SOCK_SVC_ENABLE
   //Initialize the PTT service
   if(ptt_sock_activate_svc(pHddCtx) != 0)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: ptt_sock_activate_svc failed",__func__);
      goto err_nl_srv;
   }
#endif

   if (hdd_open_cesium_nl_sock() < 0)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hdd_open_cesium_nl_sock failed", __func__);
      goto err_nl_srv;
   }

   //Initialize the CNSS-DIAG service
   if (cnss_diag_activate_service() < 0)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,
             "%s: cnss_diag_activate_service failed", __func__);
      goto err_nl_srv;
   }

   hdd_register_mcast_bcast_filter(pHddCtx);
   if (VOS_STA_SAP_MODE != hdd_get_conparam())
   {
      /* Action frame registered in one adapter which will
       * applicable to all interfaces
       */
      wlan_hdd_cfg80211_register_frames(pAdapter);
   }

   mutex_init(&pHddCtx->sap_lock);

#if defined(CONFIG_HDD_INIT_WITH_RTNL_LOCK)
   if (rtnl_lock_enable == TRUE) {
      rtnl_lock_enable = FALSE;
      rtnl_unlock();
   }
   /* register net device notifier for device change notification */
   ret = register_netdevice_notifier(&hdd_netdev_notifier);
   if (ret < 0) {
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: register_netdevice_notifier failed",
            __func__);
      goto err_nl_srv;
   }
   reg_netdev_notifier_done = TRUE;
#endif

#ifdef WLAN_FEATURE_HOLD_RX_WAKELOCK
   /* Initialize the wake lcok */
   vos_wake_lock_init(&pHddCtx->rx_wake_lock,
           "qcom_rx_wakelock");
#endif
   /* Initialize the wake lcok */
   vos_wake_lock_init(&pHddCtx->sap_wake_lock,
           "qcom_sap_wakelock");

   hdd_hostapd_channel_wakelock_init(pHddCtx);

   // Initialize the restart logic
   wlan_hdd_restart_init(pHddCtx);

   if(pHddCtx->cfg_ini->enablePowersaveOffload)
   {
      hdd_set_idle_ps_config(pHddCtx, TRUE);
   }

   if (pHddCtx->cfg_ini->enable_go_cts2self_for_sta)
       sme_set_cts2self_for_p2p_go(pHddCtx->hHal);

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
   if (pHddCtx->cfg_ini->WlanAutoShutdown != 0)
       if (sme_set_auto_shutdown_cb(pHddCtx->hHal, wlan_hdd_auto_shutdown_cb)
           != eHAL_STATUS_SUCCESS)
           hddLog(LOGE, FL("Auto shutdown feature could not be enabled"));
#endif

#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
   status = vos_timer_init(&pHddCtx->skip_acs_scan_timer, VOS_TIMER_TYPE_SW,
                  hdd_skip_acs_scan_timer_handler, (void *)pHddCtx);
   if (!VOS_IS_STATUS_SUCCESS(status))
        hddLog(LOGE, FL("Failed to init ACS Skip timer\n"));
#endif

#ifdef WLAN_FEATURE_NAN
    wlan_hdd_cfg80211_nan_init(pHddCtx);
#endif
   if (!sme_set_dense_roam_params(pHddCtx->hHal,
                  pHddCtx->cfg_ini->roam_dense_rssi_thresh_offset,
                  pHddCtx->cfg_ini->roam_dense_min_aps, 0,
                  pHddCtx->cfg_ini->roam_dense_traffic_thresh)) {
        hddLog(LOGE, FL("Failed to set dense roam params"));
   }

   /* Thermal Mitigation */
   thermalParam.smeThermalMgmtEnabled =
       pHddCtx->cfg_ini->thermalMitigationEnable;
   thermalParam.smeThrottlePeriod = pHddCtx->cfg_ini->throttlePeriod;

   thermalParam.smeThermalLevels[0].smeMinTempThreshold =
       pHddCtx->cfg_ini->thermalTempMinLevel0;
   thermalParam.smeThermalLevels[0].smeMaxTempThreshold =
       pHddCtx->cfg_ini->thermalTempMaxLevel0;
   thermalParam.smeThermalLevels[1].smeMinTempThreshold =
       pHddCtx->cfg_ini->thermalTempMinLevel1;
   thermalParam.smeThermalLevels[1].smeMaxTempThreshold =
       pHddCtx->cfg_ini->thermalTempMaxLevel1;
   thermalParam.smeThermalLevels[2].smeMinTempThreshold =
       pHddCtx->cfg_ini->thermalTempMinLevel2;
   thermalParam.smeThermalLevels[2].smeMaxTempThreshold =
       pHddCtx->cfg_ini->thermalTempMaxLevel2;
   thermalParam.smeThermalLevels[3].smeMinTempThreshold =
       pHddCtx->cfg_ini->thermalTempMinLevel3;
   thermalParam.smeThermalLevels[3].smeMaxTempThreshold =
       pHddCtx->cfg_ini->thermalTempMaxLevel3;

   if (eHAL_STATUS_SUCCESS != sme_InitThermalInfo(pHddCtx->hHal,thermalParam))
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: Error while initializing thermal information", __func__);
   }

   /* Plug in set thermal level callback */
   sme_add_set_thermal_level_callback(pHddCtx->hHal,
                     (tSmeSetThermalLevelCallback)hdd_set_thermal_level_cb);

   /* Bad peer tx flow control */
   wlan_hdd_bad_peer_txctl(pHddCtx);

   /* SAR power limit */
   hddtxlimit = vos_mem_malloc(sizeof(tSirTxPowerLimit));
   if (!hddtxlimit)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: Memory allocation for TxPowerLimit "
                 "failed!", __func__);
       goto err_nl_srv;
   }
   hddtxlimit->txPower2g = pHddCtx->cfg_ini->TxPower2g;
   hddtxlimit->txPower5g = pHddCtx->cfg_ini->TxPower5g;

   if (eHAL_STATUS_SUCCESS != sme_TxpowerLimit(pHddCtx->hHal,hddtxlimit))
   {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: Error setting txlimit in sme", __func__);
   }

   hdd_tsf_init(pHddCtx);

#ifdef FEATURE_BUS_BANDWIDTH
   spin_lock_init(&pHddCtx->bus_bw_lock);
   vos_timer_init(&pHddCtx->bus_bw_timer,
                     VOS_TIMER_TYPE_SW,
                     hdd_bus_bw_compute_cbk,
                     (void *)pHddCtx);
#endif

#ifdef WLAN_FEATURE_STATS_EXT
   wlan_hdd_cfg80211_stats_ext_init(pHddCtx);
#endif
#ifdef FEATURE_WLAN_EXTSCAN
    sme_ExtScanRegisterCallback(pHddCtx->hHal,
                                wlan_hdd_cfg80211_extscan_callback);
#endif /* FEATURE_WLAN_EXTSCAN */
    sme_set_rssi_threshold_breached_cb(pHddCtx->hHal, hdd_rssi_threshold_breached);
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
   wlan_hdd_cfg80211_link_layer_stats_init(pHddCtx);
#endif

   wlan_hdd_tsf_init(pHddCtx);

#ifdef WLAN_FEATURE_LPSS
   wlan_hdd_send_all_scan_intf_info(pHddCtx);
   wlan_hdd_send_version_pkg(pHddCtx->target_fw_version,
                             pHddCtx->target_hw_version,
                             pHddCtx->target_hw_name);
#endif

   if (WLAN_HDD_RX_HANDLE_RPS == pHddCtx->cfg_ini->rxhandle)
      hdd_dp_util_send_rps_ind(pHddCtx);

   hal_status = sme_set_lost_link_info_cb(pHddCtx->hHal,
                                          hdd_lost_link_info_cb);
   /* print error and not block the startup process */
   if (eHAL_STATUS_SUCCESS != hal_status)
       hddLog(LOGE, "%s: set lost link info callback failed", __func__);

   hal_status = sme_set_smps_force_mode_cb(pHddCtx->hHal,
                                           hdd_smps_force_mode_cb);
   if (eHAL_STATUS_SUCCESS != hal_status)
       hddLog(LOGE, FL("set smps force mode callback failed"));

   hal_status = sme_bpf_offload_register_callback(pHddCtx->hHal,
                                    hdd_get_bpf_offload_cb);
   if (eHAL_STATUS_SUCCESS != hal_status)
       hddLog(LOGE, FL("set bpf offload callback failed"));

   /* Initialize the RoC Request queue and work. */
   hdd_list_init((&pHddCtx->hdd_roc_req_q), MAX_ROC_REQ_QUEUE_ENTRY);
   vos_init_delayed_work(&pHddCtx->rocReqWork, wlan_hdd_roc_request_dequeue);

   wlan_hdd_dcc_register_for_dcc_stats_event(pHddCtx);

   /*
    * Register IPv6 notifier to notify if any change in IP
    * So that we can reconfigure the offload parameters
    */
   hdd_wlan_register_ip6_notifier(pHddCtx);

   /*
    * Register IPv4 notifier to notify if any change in IP
    * So that we can reconfigure the offload parameters
    */
   pHddCtx->ipv4_notifier.notifier_call = wlan_hdd_ipv4_changed;
   ret = register_inetaddr_notifier(&pHddCtx->ipv4_notifier);
   if (ret)
      hddLog(LOGE, FL("Failed to register IPv4 notifier"));
   else
      hddLog(LOG1, FL("Registered IPv4 notifier"));

   ol_pktlog_init(hif_sc);
   hdd_runtime_suspend_init(pHddCtx);
   pHddCtx->isLoadInProgress = FALSE;
   vos_set_load_unload_in_progress(VOS_MODULE_ID_VOSS, FALSE);
   vos_set_load_in_progress(VOS_MODULE_ID_VOSS, FALSE);
   memdump_init();
   complete(&wlan_start_comp);
   goto success;

err_nl_srv:
#ifdef WLAN_KD_READY_NOTIFIER
   cnss_diag_notify_wlan_close();
   nl_srv_exit(pHddCtx->ptt_pid);
#else
   nl_srv_exit();
#endif /* WLAN_KD_READY_NOTIFIER */

   hdd_close_cesium_nl_sock();

err_reg_netdev:
   if (rtnl_lock_enable == TRUE) {
      rtnl_lock_enable = FALSE;
      rtnl_unlock();
   }
   if (reg_netdev_notifier_done == TRUE) {
      unregister_netdevice_notifier(&hdd_netdev_notifier);
      reg_netdev_notifier_done = FALSE;
   }

#if !defined(CONFIG_HDD_INIT_WITH_RTNL_LOCK)
err_free_power_on_lock:
#endif
   free_riva_power_on_lock("wlan");

err_unregister_pmops:
   hddDevTmUnregisterNotifyCallback(pHddCtx);
   hddDeregisterPmOps(pHddCtx);

   hdd_debugfs_exit(pHddCtx);



err_close_adapter:
#if defined(CONFIG_HDD_INIT_WITH_RTNL_LOCK)
   if (rtnl_lock_enable == TRUE) {
      rtnl_lock_enable = FALSE;
      rtnl_unlock();
   }
#endif
   hdd_close_all_adapters( pHddCtx );

err_vosstop:
   vos_stop(pVosContext);

#ifdef IPA_OFFLOAD
err_ipa_cleanup:
   hdd_ipa_cleanup(pHddCtx);
#endif

err_wiphy_unregister:
   wiphy_unregister(wiphy);

err_vosclose:
   status = vos_sched_close( pVosContext );
   if (!VOS_IS_STATUS_SUCCESS(status))    {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
         "%s: Failed to close VOSS Scheduler", __func__);
      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( status ) );
   }
   vos_close(pVosContext );

err_vos_nv_close:

   vos_nv_close();

   hdd_wlan_green_ap_deinit(pHddCtx);

err_wdclose:
   if(pHddCtx->cfg_ini->fIsLogpEnabled)
      vos_watchdog_close(pVosContext);

if (VOS_FTM_MODE == hdd_get_conparam())
{
#if  defined(QCA_WIFI_FTM)
err_free_ftm_open:
   wlan_hdd_ftm_close(pHddCtx);
#endif
}

   if (VOS_FTM_MODE != hdd_get_conparam())
       wlan_hdd_logging_sock_deactivate_svc(pHddCtx);

err_config:
   kfree(pHddCtx->cfg_ini);
   pHddCtx->cfg_ini= NULL;

err_histogram:
   wlan_hdd_deinit_tx_rx_histogram(pHddCtx);

err_free_hdd_context:
   /* wiphy_free() will free the HDD context so remove global reference */
   if (pVosContext)
      ((VosContextType*)(pVosContext))->pHDDContext = NULL;

   wiphy_free(wiphy) ;
   //kfree(wdev) ;
   VOS_BUG(1);

   if (hdd_is_ssr_required())
   {
#ifdef MSM_PLATFORM
#ifdef CONFIG_CNSS
       /* WDI timeout had happened during load, so SSR is needed here */
       subsystem_restart("wcnss");
#endif
#endif
       msleep(5000);
   }
   hdd_set_ssr_required (VOS_FALSE);

   return -EIO;

success:
   EXIT();
   return 0;
}

/**---------------------------------------------------------------------------

  \brief hdd_driver_init() - Core Driver Init Function

   This is the driver entry point - called in different time line depending
   on whether the driver is statically or dynamically linked

  \param  - None

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/
static int hdd_driver_init( void)
{
   VOS_STATUS status;
   v_CONTEXT_t pVosContext = NULL;
   int ret_status = 0;
   unsigned long rc;
   u_int64_t start;

   start = adf_get_boottime();

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
   wlan_logging_sock_init_svc();
#endif

   ENTER();

#ifdef TIMER_MANAGER
      vos_timer_manager_init();
#endif

#ifdef MEMORY_DEBUG
      vos_mem_init();
#endif

   vos_wake_lock_init(&wlan_wake_lock, "wlan");
   hdd_prevent_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_INIT);

   /*
    * The Krait is going to Idle/Stand Alone Power Save
    * more aggressively which is resulting in the longer driver load time.
    * The Fix is to not allow Krait to enter Idle Power Save during driver load.
    * at a time, and wait for the completion interrupt to start the next
    * transfer. During this phase, the KRAIT is entering IDLE/StandAlone(SA)
    * Power Save(PS). The delay incurred for resuming from IDLE/SA PS is
    * huge during driver load. So prevent APPS IDLE/SA PS during driver
    * load for reducing interrupt latency.
    */

   vos_request_pm_qos(DISABLE_KRAIT_IDLE_PS_VAL);

   vos_ssr_protect_init();

   pr_info("%s: loading driver v%s\n", WLAN_MODULE_NAME,
           QWLAN_VERSIONSTR TIMER_MANAGER_STR MEMORY_DEBUG_STR);

   do {

#ifndef MODULE
      if (WLAN_IS_EPPING_ENABLED(con_mode)) {
         ret_status =  epping_driver_init(con_mode, &wlan_wake_lock,
                          WLAN_MODULE_NAME);
         if (ret_status < 0) {
            vos_remove_pm_qos();
            hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_INIT);
            vos_wake_lock_destroy(&wlan_wake_lock);
         }
         return ret_status;
      }
#else
      if (WLAN_IS_EPPING_ENABLED(hdd_get_conparam())) {
         ret_status = epping_driver_init(hdd_get_conparam(),
                         &wlan_wake_lock, WLAN_MODULE_NAME);
         if (ret_status < 0) {
            vos_remove_pm_qos();
            hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_INIT);
            vos_wake_lock_destroy(&wlan_wake_lock);
         }
         return ret_status;
      }
#endif

      /* Preopen VOSS so that it is ready to start at least SAL */
      status = vos_preOpen(&pVosContext);

   if (!VOS_IS_STATUS_SUCCESS(status))
   {
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Failed to preOpen VOSS", __func__);
         ret_status = -1;
         break;
   }

#ifdef HDD_TRACE_RECORD
   MTRACE(hddTraceInit());
#endif

#ifndef MODULE
      /* For statically linked driver, call hdd_set_conparam to update curr_con_mode
       */
      hdd_set_conparam((v_UINT_t)con_mode);
#endif

#define HDD_WLAN_START_WAIT_TIME VOS_WDA_TIMEOUT + 5000

   init_completion(&wlan_start_comp);

   ret_status = hif_register_driver();
   if (!ret_status) {
       rc = wait_for_completion_timeout(
                           &wlan_start_comp,
                           msecs_to_jiffies(HDD_WLAN_START_WAIT_TIME));
       if (!rc) {
          hddLog(VOS_TRACE_LEVEL_FATAL,
            "%s: timed-out waiting for hif_register_driver", __func__);
           ret_status = -1;
       } else
           ret_status = 0;
   }

   vos_remove_pm_qos();
   hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_INIT);

   if (ret_status) {
       hddLog(VOS_TRACE_LEVEL_FATAL, "%s: WLAN Driver Initialization failed",
               __func__);
       hif_unregister_driver();
       vos_preClose( &pVosContext );
       ret_status = -ENODEV;
       break;
   } else {
       pr_info("%s: driver loaded in %lld\n", WLAN_MODULE_NAME,
                                              adf_get_boottime() - start);
       return 0;
   }


   } while (0);

   if (0 != ret_status)
   {
#ifdef TIMER_MANAGER
      vos_timer_exit();
#endif
#ifdef MEMORY_DEBUG
      vos_mem_exit();
#endif
      vos_wake_lock_destroy(&wlan_wake_lock);

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
      wlan_logging_sock_deinit_svc();
#endif
      pr_err("%s: driver load failure\n", WLAN_MODULE_NAME);
   }

   EXIT();

   return ret_status;
}

/**---------------------------------------------------------------------------

  \brief hdd_module_init() - Init Function

   This is the driver entry point (invoked when module is loaded using insmod)

  \param  - None

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/
#ifdef MODULE
static int __init hdd_module_init ( void)
{
   return hdd_driver_init();
}
#else /* #ifdef MODULE */
static int __init hdd_module_init ( void)
{
   /* Driver initialization is delayed to fwpath_changed_handler */
   return 0;
}
#endif /* #ifdef MODULE */

/**---------------------------------------------------------------------------

  \brief hdd_driver_exit() - Exit function

  This is the driver exit point (invoked when module is unloaded using rmmod
  or con_mode was changed by user space)

  \param  - None

  \return - None

  --------------------------------------------------------------------------*/
static void hdd_driver_exit(void)
{
   hdd_context_t *pHddCtx = NULL;
   int retry = 0;
   v_CONTEXT_t pVosContext = NULL;

   pr_info("%s: unloading driver v%s\n", WLAN_MODULE_NAME, QWLAN_VERSIONSTR);

   //Get the global vos context
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

   if(!pVosContext)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Global VOS context is Null", __func__);
      goto done;
   }

   if (WLAN_IS_EPPING_ENABLED(con_mode)) {
      epping_driver_exit(pVosContext);
      goto done;
   }

   //Get the HDD context.
   pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext );

   if(!pHddCtx)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: module exit called before probe",__func__);
   }
   else
   {
      /*
       * Check IPA HW pipe shutdown properly or not
       * If not, force shut down HW pipe
       */
      hdd_ipa_uc_force_pipe_shutdown(pHddCtx);

#ifdef QCA_PKT_PROTO_TRACE
      vos_pkt_proto_trace_close();
#endif /* QCA_PKT_PROTO_TRACE */
      while(pHddCtx->isLogpInProgress ||
            vos_is_logp_in_progress(VOS_MODULE_ID_VOSS, NULL)) {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
              "%s:SSR in Progress; block rmmod for 1 second!!!", __func__);
         msleep(1000);

         if (retry++ == HDD_MOD_EXIT_SSR_MAX_RETRIES) {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
              "%s:SSR never completed, fatal error", __func__);
            VOS_BUG(0);
         }
      }

      rtnl_lock();
      pHddCtx->isUnloadInProgress = TRUE;
      vos_set_load_unload_in_progress(VOS_MODULE_ID_VOSS, TRUE);
      vos_set_unload_in_progress(TRUE);
      rtnl_unlock();
   }

   vos_wait_for_work_thread_completion(__func__);

   hif_unregister_driver();

   vos_preClose( &pVosContext );

#ifdef TIMER_MANAGER
   vos_timer_exit();
#endif
#ifdef MEMORY_DEBUG
   vos_mem_exit();
#endif

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
   wlan_logging_sock_deinit_svc();
#endif

done:
   vos_wake_lock_destroy(&wlan_wake_lock);
   pr_info("%s: driver unloaded\n", WLAN_MODULE_NAME);
}

/**---------------------------------------------------------------------------

  \brief hdd_module_exit() - Exit function

  This is the driver exit point (invoked when module is unloaded using rmmod)

  \param  - None

  \return - None

  --------------------------------------------------------------------------*/
static void __exit hdd_module_exit(void)
{
   hdd_driver_exit();
}

#ifdef MODULE
static int fwpath_changed_handler(const char *kmessage,
                                 struct kernel_param *kp)
{
   return param_set_copystring(kmessage, kp);
}

#if ! defined(QCA_WIFI_FTM)
static int con_mode_handler(const char *kmessage,
                                 struct kernel_param *kp)
{
   return param_set_int(kmessage, kp);
}
#endif
#else /* #ifdef MODULE */

/**---------------------------------------------------------------------------

  \brief fwpath_changed_handler() - Handler Function

   Handle changes to the fwpath parameter

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/
static int fwpath_changed_handler(const char *kmessage,
                                  struct kernel_param *kp)
{
   int ret;

   ret = param_set_copystring(kmessage, kp);
   if (0 == ret)
      ret = kickstart_driver(true);
   return ret;
}

#if ! defined(QCA_WIFI_FTM)
/**---------------------------------------------------------------------------

  \brief con_mode_handler() -

  Handler function for module param con_mode when it is changed by user space
  Dynamically linked - do nothing
  Statically linked - exit and init driver, as in rmmod and insmod

  \param  -

  \return -

  --------------------------------------------------------------------------*/
static int con_mode_handler(const char *kmessage, struct kernel_param *kp)
{
   int ret;

   ret = param_set_int(kmessage, kp);
   if (0 == ret)
      ret = kickstart_driver(true);
   return ret;
}
#endif
#endif /* #ifdef MODULE */

/**---------------------------------------------------------------------------

  \brief hdd_get_conparam() -

  This is the driver exit point (invoked when module is unloaded using rmmod)

  \param  - None

  \return - tVOS_CON_MODE

  --------------------------------------------------------------------------*/
tVOS_CON_MODE hdd_get_conparam ( void )
{
#ifdef MODULE
    return (tVOS_CON_MODE)con_mode;
#else
    return (tVOS_CON_MODE)curr_con_mode;
#endif
}
void hdd_set_conparam ( v_UINT_t newParam )
{
  con_mode = newParam;
#ifndef MODULE
  curr_con_mode = con_mode;
#endif
}
/**---------------------------------------------------------------------------

  \brief hdd_softap_sta_deauth() - function

  This to take counter measure to handle deauth req from HDD

  \param  - pAdapter - Pointer to the HDD

  \param  - enable - boolean value

  \return - None

  --------------------------------------------------------------------------*/

VOS_STATUS hdd_softap_sta_deauth(hdd_adapter_t *pAdapter,
                                 struct tagCsrDelStaParams *pDelStaParams)
{
#ifndef WLAN_FEATURE_MBSSID
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pAdapter))->pvosContext;
#endif
    VOS_STATUS vosStatus = VOS_STATUS_E_FAULT;

    ENTER();

    hddLog(LOG1, "hdd_softap_sta_deauth:(%p, false)",
           (WLAN_HDD_GET_CTX(pAdapter))->pvosContext);

    //Ignore request to deauth bcmc station
    if (pDelStaParams->peerMacAddr[0] & 0x1)
       return vosStatus;

#ifdef WLAN_FEATURE_MBSSID
    vosStatus = WLANSAP_DeauthSta(WLAN_HDD_GET_SAP_CTX_PTR(pAdapter),
                                  pDelStaParams);
#else
    vosStatus = WLANSAP_DeauthSta(pVosContext, pDelStaParams);
#endif

    EXIT();
    return vosStatus;
}

/**---------------------------------------------------------------------------

  \brief hdd_softap_sta_disassoc() - function

  This to take counter measure to handle deauth req from HDD

  \param  - pAdapter - Pointer to the HDD

  \param  - enable - boolean value

  \return - None

  --------------------------------------------------------------------------*/

void hdd_softap_sta_disassoc(hdd_adapter_t *pAdapter,
                             struct tagCsrDelStaParams *pDelStaParams)
{
#ifndef WLAN_FEATURE_MBSSID
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pAdapter))->pvosContext;
#endif

    ENTER();

    hddLog( LOGE, "hdd_softap_sta_disassoc:(%p, false)", (WLAN_HDD_GET_CTX(pAdapter))->pvosContext);

    //Ignore request to disassoc bcmc station
    if( pDelStaParams->peerMacAddr[0] & 0x1 )
       return;

#ifdef WLAN_FEATURE_MBSSID
    WLANSAP_DisassocSta(WLAN_HDD_GET_SAP_CTX_PTR(pAdapter), pDelStaParams);
#else
    WLANSAP_DisassocSta(pVosContext, pDelStaParams);
#endif
}

void hdd_softap_tkip_mic_fail_counter_measure(hdd_adapter_t *pAdapter,v_BOOL_t enable)
{
#ifndef WLAN_FEATURE_MBSSID
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pAdapter))->pvosContext;
#endif

    ENTER();

    hddLog( LOGE, "hdd_softap_tkip_mic_fail_counter_measure:(%p, false)", (WLAN_HDD_GET_CTX(pAdapter))->pvosContext);

#ifdef WLAN_FEATURE_MBSSID
    WLANSAP_SetCounterMeasure(WLAN_HDD_GET_SAP_CTX_PTR(pAdapter), (v_BOOL_t)enable);
#else
    WLANSAP_SetCounterMeasure(pVosContext, (v_BOOL_t)enable);
#endif
}

/**---------------------------------------------------------------------------
 *
 *   \brief hdd_get__concurrency_mode() -
 *
 *
 *   \param  - None
 *
 *   \return - CONCURRENCY MODE
 *
 * --------------------------------------------------------------------------*/
tVOS_CONCURRENCY_MODE hdd_get_concurrency_mode ( void )
{
    v_CONTEXT_t pVosContext = vos_get_global_context( VOS_MODULE_ID_HDD, NULL );
    hdd_context_t *pHddCtx;

    if (NULL != pVosContext)
    {
       pHddCtx = vos_get_context( VOS_MODULE_ID_HDD, pVosContext);
       if (NULL != pHddCtx)
       {
          hddLog(VOS_TRACE_LEVEL_INFO, "%s: concurrency_mode = 0x%x", __func__,
                                        pHddCtx->concurrency_mode);
          return (tVOS_CONCURRENCY_MODE)pHddCtx->concurrency_mode;
       }
    }

    /* we are in an invalid state :( */
    hddLog(LOGE, "%s: Invalid context", __func__);
    return VOS_STA;
}

/* Decides whether to send suspend notification to Riva
 * if any adapter is in BMPS; then it is required */
v_BOOL_t hdd_is_suspend_notify_allowed(hdd_context_t* pHddCtx)
{
    tPmcState pmcState = pmcGetPmcState(pHddCtx->hHal);
    hdd_config_t *pConfig = pHddCtx->cfg_ini;

    if (pConfig->fIsBmpsEnabled && (pmcState == BMPS))
    {
        return TRUE;
    }
    return FALSE;
}

void wlan_hdd_set_concurrency_mode(hdd_context_t *pHddCtx, tVOS_CON_MODE mode)
{
   switch (mode) {
       case VOS_STA_MODE:
       case VOS_P2P_CLIENT_MODE:
       case VOS_P2P_GO_MODE:
       case VOS_STA_SAP_MODE:
            pHddCtx->concurrency_mode |= (1 << mode);
            pHddCtx->no_of_open_sessions[mode]++;
            break;
       default:
            break;
   }
   hddLog(VOS_TRACE_LEVEL_INFO, FL("concurrency_mode = 0x%x "
          "Number of open sessions for mode %d = %d"),
           pHddCtx->concurrency_mode, mode,
           pHddCtx->no_of_open_sessions[mode]);
}


void wlan_hdd_clear_concurrency_mode(hdd_context_t *pHddCtx, tVOS_CON_MODE mode)
   {
   switch (mode)  {
       case VOS_STA_MODE:
       case VOS_P2P_CLIENT_MODE:
       case VOS_P2P_GO_MODE:
       case VOS_STA_SAP_MODE:
            pHddCtx->no_of_open_sessions[mode]--;
            if (!(pHddCtx->no_of_open_sessions[mode]))
                pHddCtx->concurrency_mode &= (~(1 << mode));
            break;
       default:
            break;
   }
   hddLog(VOS_TRACE_LEVEL_INFO, FL("concurrency_mode = 0x%x "
          "Number of open sessions for mode %d = %d"),
          pHddCtx->concurrency_mode, mode, pHddCtx->no_of_open_sessions[mode]);
   }

/**---------------------------------------------------------------------------
 *
 *   \brief wlan_hdd_incr_active_session()
 *
 *   This function increments the number of active sessions
 *   maintained per device mode
 *   Incase of STA/P2P CLI/IBSS upon connection indication it is incremented
 *   Incase of SAP/P2P GO upon bss start it is incremented
 *
 *   \param  pHddCtx - HDD Context
 *   \param  mode    - device mode
 *
 *   \return - None
 *
 * --------------------------------------------------------------------------*/
void wlan_hdd_incr_active_session(hdd_context_t *pHddCtx, tVOS_CON_MODE mode)
{
   switch (mode) {
   case VOS_STA_MODE:
   case VOS_P2P_CLIENT_MODE:
   case VOS_P2P_GO_MODE:
   case VOS_STA_SAP_MODE:
        pHddCtx->no_of_active_sessions[mode]++;
        break;
   default:
        break;
   }
   hddLog(VOS_TRACE_LEVEL_INFO, FL("No.# of active sessions for mode %d = %d"),
                                mode,
                                pHddCtx->no_of_active_sessions[mode]);
}

/**---------------------------------------------------------------------------
 *
 *   \brief wlan_hdd_decr_active_session()
 *
 *   This function decrements the number of active sessions
 *   maintained per device mode
 *   Incase of STA/P2P CLI/IBSS upon disconnection it is decremented
 *   Incase of SAP/P2P GO upon bss stop it is decremented
 *
 *   \param  pHddCtx - HDD Context
 *   \param  mode    - device mode
 *
 *   \return - None
 *
 * --------------------------------------------------------------------------*/
void wlan_hdd_decr_active_session(hdd_context_t *pHddCtx, tVOS_CON_MODE mode)
{
   switch (mode) {
   case VOS_STA_MODE:
   case VOS_P2P_CLIENT_MODE:
   case VOS_P2P_GO_MODE:
   case VOS_STA_SAP_MODE:
        if (pHddCtx->no_of_active_sessions[mode])
            pHddCtx->no_of_active_sessions[mode]--;
        break;
   default:
        break;
   }
   hddLog(VOS_TRACE_LEVEL_INFO, FL("No.# of active sessions for mode %d = %d"),
                                mode,
                                pHddCtx->no_of_active_sessions[mode]);
}

/**
 * wlan_hdd_get_active_session_count() - get active session
 * connection count
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: count of active connections
 */
uint8_t wlan_hdd_get_active_session_count(hdd_context_t *hdd_ctx)
{
	uint8_t i = 0;
	uint8_t count = 0;

	for (i = 0; i < VOS_MAX_NO_OF_MODE; i++) {
		count += hdd_ctx->no_of_active_sessions[i];
	}
	return count;
}

/**
 * wlan_hdd_update_txrx_chain_mask() - updates the TX/RX chain
 * mask to FW
 * @hdd_ctx: Pointer to hdd context
 * @chain_mask : Vlaue of the chain_mask to be updated
 *
 * Return: 0 for success non-zero for failure
 */
int wlan_hdd_update_txrx_chain_mask(hdd_context_t *hdd_ctx,
				    uint8_t chain_mask)
{
	int ret;

	if (hdd_ctx->per_band_chainmask_supp == 1) {
		ret = process_wma_set_command(0,
				WMI_PDEV_PARAM_RX_CHAIN_MASK_2G,
				chain_mask, PDEV_CMD);
		if (0 != ret) {
			hddLog(LOGE, FL("Failed to set 2G RX chain mask: %d"),
			       chain_mask);
			return -EFAULT;
		}
		ret = process_wma_set_command(0,
				WMI_PDEV_PARAM_TX_CHAIN_MASK_2G,
				chain_mask, PDEV_CMD);
		if (0 != ret) {
			hddLog(LOGE, FL("Failed to set 2G TX chain mask: %d"),
			       chain_mask);
			return -EFAULT;
		}

		ret = process_wma_set_command(0,
				WMI_PDEV_PARAM_RX_CHAIN_MASK_5G,
				chain_mask, PDEV_CMD);
		if (0 != ret) {
			hddLog(LOGE, FL("Failed to set 5G RX chain mask: %d"),
			       chain_mask);
			return -EFAULT;
		}
		ret = process_wma_set_command(0,
				WMI_PDEV_PARAM_TX_CHAIN_MASK_5G,
				chain_mask, PDEV_CMD);
		if (0 != ret) {
			hddLog(LOGE, FL("Failed to set 5G TX chain mask: %d"),
			       chain_mask);
			return -EFAULT;
		}
	} else {
		ret = process_wma_set_command(0,
					WMI_PDEV_PARAM_RX_CHAIN_MASK,
					chain_mask, PDEV_CMD);
		if (0 != ret) {
			hddLog(LOGE, FL("Failed to set RX chain mask: %d"),
			       chain_mask);
			return -EFAULT;
		}

		ret = process_wma_set_command(0,
					WMI_PDEV_PARAM_TX_CHAIN_MASK,
					chain_mask, PDEV_CMD);
		if (0 != ret) {
			hddLog(LOGE, FL("Failed to set TX chain mask: %d"),
			       chain_mask);
			return -EFAULT;
		}
	}

	hddLog(LOG1, FL("Sucessfully updated the TX/RX chain mask to: %d"),
	       chain_mask);
	return 0;
}

/**---------------------------------------------------------------------------
 *
 *   \brief wlan_hdd_framework_restart
 *
 *   This function uses a cfg80211 API to start a framework initiated WLAN
 *   driver module unload/load.
 *
 *   Also this API keep retrying (WLAN_HDD_RESTART_RETRY_MAX_CNT).
 *
 *
 *   \param  - pHddCtx
 *
 *   \return - VOS_STATUS_SUCCESS: Success
 *             VOS_STATUS_E_EMPTY: Adapter is Empty
 *             VOS_STATUS_E_NOMEM: No memory

 * --------------------------------------------------------------------------*/

static VOS_STATUS wlan_hdd_framework_restart(hdd_context_t *pHddCtx)
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   int len = (sizeof (struct ieee80211_mgmt));
   struct ieee80211_mgmt *mgmt = NULL;

   /* Prepare the DEAUTH management frame with reason code */
   mgmt =  kzalloc(len, GFP_KERNEL);
   if(mgmt == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
            "%s: memory allocation failed (%d bytes)", __func__, len);
      return VOS_STATUS_E_NOMEM;
   }
   mgmt->u.deauth.reason_code = WLAN_REASON_DISASSOC_LOW_ACK;

   /* Iterate over all adapters/devices */
   status =  hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
   if ((NULL == pAdapterNode) || (VOS_STATUS_SUCCESS != status)) {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("fail to get adapter: %p %d"), pAdapterNode, status);
       goto end;
   }
   do
   {
      if(pAdapterNode->pAdapter &&
           WLAN_HDD_ADAPTER_MAGIC == pAdapterNode->pAdapter->magic) {
         hddLog(LOGP,
               "restarting the driver(intf:\'%s\' mode:%s(%d) :try %d)",
               pAdapterNode->pAdapter->dev->name,
               hdd_device_mode_to_string(pAdapterNode->pAdapter->device_mode),
               pAdapterNode->pAdapter->device_mode,
               pHddCtx->hdd_restart_retries + 1);
         /*
          * CFG80211 event to restart the driver
          *
          * 'cfg80211_send_unprot_deauth' sends a
          * NL80211_CMD_UNPROT_DEAUTHENTICATE event to supplicant at any state
          * of SME(Linux Kernel) state machine.
          *
          * Reason code WLAN_REASON_DISASSOC_LOW_ACK is currently used to restart
          * the driver.
          *
          */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0)) || defined(WITH_BACKPORTS)
         cfg80211_rx_unprot_mlme_mgmt(pAdapterNode->pAdapter->dev,
                                      (u_int8_t*)mgmt, len);
#else
         cfg80211_send_unprot_deauth(pAdapterNode->pAdapter->dev,
                                     (u_int8_t*)mgmt, len);
#endif
      }
      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   } while((NULL != pAdapterNode) && (VOS_STATUS_SUCCESS == status));

   end:
   /* Free the allocated management frame */
   kfree(mgmt);

   /* Retry until we unload or reach max count */
   if(++pHddCtx->hdd_restart_retries < WLAN_HDD_RESTART_RETRY_MAX_CNT)
      vos_timer_start(&pHddCtx->hdd_restart_timer, WLAN_HDD_RESTART_RETRY_DELAY_MS);

   return status;

}
/**---------------------------------------------------------------------------
 *
 *   \brief wlan_hdd_restart_timer_cb
 *
 *   Restart timer callback. An internal function.
 *
 *   \param  - User data:
 *
 *   \return - None
 *
 * --------------------------------------------------------------------------*/

void wlan_hdd_restart_timer_cb(v_PVOID_t usrDataForCallback)
{
   hdd_context_t *pHddCtx = usrDataForCallback;
   wlan_hdd_framework_restart(pHddCtx);
   return;

}


/**---------------------------------------------------------------------------
 *
 *   \brief wlan_hdd_restart_driver
 *
 *   This function sends an event to supplicant to restart the WLAN driver.
 *
 *   This function is called from vos_wlanRestart.
 *
 *   \param  - pHddCtx
 *
 *   \return - VOS_STATUS_SUCCESS: Success
 *             VOS_STATUS_E_EMPTY: Adapter is Empty
 *             VOS_STATUS_E_ALREADY: Request already in progress

 * --------------------------------------------------------------------------*/
VOS_STATUS wlan_hdd_restart_driver(hdd_context_t *pHddCtx)
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;

   /* A tight check to make sure reentrancy */
   if(atomic_xchg(&pHddCtx->isRestartInProgress, 1))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
            "%s: WLAN restart is already in progress", __func__);

      return VOS_STATUS_E_ALREADY;
   }
   /* Send reset FIQ to WCNSS to invoke SSR. */
#ifdef HAVE_WCNSS_RESET_INTR
   wcnss_reset_intr();
#endif

   return status;
}

/*
 * API to find if there is any STA or P2P-Client is connected
 */
VOS_STATUS hdd_issta_p2p_clientconnected(hdd_context_t *pHddCtx)
{
    return sme_isSta_p2p_clientConnected(pHddCtx->hHal);
}

#ifdef FEATURE_WLAN_CH_AVOID
/**
 * hdd_find_prefd_safe_chnl : Finds safe channel within preferred channel
 * @hdd_ctxt: hdd context pointer
 * @ap_adapter: hdd hostapd adapter pointer
 *
 * If auto channel selection enabled:
 * Preferred and safe channel should be used
 * If no overlapping, preferred channel should be used
 *
 * Return:
 * 1: found preferred safe channel
 * 0: could not found preferred safe channel
 */

static uint8_t hdd_find_prefd_safe_chnl(hdd_context_t *hdd_ctxt,
                                                  hdd_adapter_t *ap_adapter)
{
   uint16_t             safe_channels[NUM_20MHZ_RF_CHANNELS];
   uint16_t             safe_channel_count;
   uint16_t             unsafe_channel_count;
   uint8_t              is_unsafe = 1;
   uint16_t             i;
   uint16_t             channel_loop;

   if (!hdd_ctxt || !ap_adapter) {
      hddLog(LOGE, "%s : Invalid arguments: hdd_ctxt=%p, ap_adapter=%p",
             __func__, hdd_ctxt, ap_adapter);
      return 0;
   }

   safe_channel_count = 0;
   unsafe_channel_count = VOS_MIN((uint16_t)hdd_ctxt->unsafe_channel_count,
                              (uint16_t)NUM_20MHZ_RF_CHANNELS);

   for (i = 0; i < NUM_20MHZ_RF_CHANNELS; i++) {
      is_unsafe = 0;
      for (channel_loop = 0;
           channel_loop < unsafe_channel_count; channel_loop++) {
         if (rfChannels[i].channelNum ==
             hdd_ctxt->unsafe_channel_list[channel_loop]) {
            is_unsafe = 1;
            break;
         }
      }
      if (!is_unsafe) {
         safe_channels[safe_channel_count] = rfChannels[i].channelNum;
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
             "safe channel %d", safe_channels[safe_channel_count]);
         safe_channel_count++;
      }
  }

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
             "perferred range %d - %d",
             ap_adapter->sessionCtx.ap.sapConfig.acs_cfg.start_ch,
             ap_adapter->sessionCtx.ap.sapConfig.acs_cfg.end_ch);
   for (i = 0; i < safe_channel_count; i++) {
      if ((safe_channels[i] >=
                    ap_adapter->sessionCtx.ap.sapConfig.acs_cfg.start_ch) &&
          (safe_channels[i] <=
                    ap_adapter->sessionCtx.ap.sapConfig.acs_cfg.end_ch)) {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
             "safe channel %d is in perferred range", safe_channels[i]);
         return 1;
      }
   }

   return 0;
}

/**---------------------------------------------------------------------------

  \brief hdd_ch_avoid_cb() -

  Avoid channel notification from FW handler.
  FW will send un-safe channel list to avoid over wrapping.
  hostapd should not use notified channel

  \param  - pAdapter HDD adapter pointer
            indParam channel avoid notification parameter

  \return - None

  --------------------------------------------------------------------------*/
void hdd_ch_avoid_cb
(
   void *hdd_context,
   void *indi_param
)
{
   hdd_adapter_t      *hostapd_adapter = NULL;
   hdd_context_t      *hdd_ctxt;
   tSirChAvoidIndType *ch_avoid_indi;
   v_U8_t              range_loop;
   eRfChannels         channel_loop, start_channel_idx = INVALID_RF_CHANNEL,
                                     end_channel_idx = INVALID_RF_CHANNEL;
   v_U16_t             start_channel;
   v_U16_t             end_channel;
   v_CONTEXT_t         vos_context;
   static int          restart_sap_in_progress = 0;
   tHddAvoidFreqList   hdd_avoid_freq_list;
   tANI_U32            i;
   hdd_adapter_list_node_t *adapter_node = NULL, *next = NULL;
   VOS_STATUS status;

   /* Basic sanity */
   if (!hdd_context || !indi_param)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s : Invalid arguments", __func__);
      return;
   }

   hdd_ctxt    = (hdd_context_t *)hdd_context;
   ch_avoid_indi  = (tSirChAvoidIndType *)indi_param;
   vos_context = hdd_ctxt->pvosContext;

   /* Make unsafe channel list */
   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
             "%s : band count %d",
             __func__, ch_avoid_indi->avoid_range_count);

   /* generate vendor specific event */
   vos_mem_zero((void *)&hdd_avoid_freq_list, sizeof(tHddAvoidFreqList));
   for (i = 0; i < ch_avoid_indi->avoid_range_count; i++)
   {
      hdd_avoid_freq_list.avoidFreqRange[i].startFreq =
            ch_avoid_indi->avoid_freq_range[i].start_freq;
      hdd_avoid_freq_list.avoidFreqRange[i].endFreq =
            ch_avoid_indi->avoid_freq_range[i].end_freq;
   }
   hdd_avoid_freq_list.avoidFreqRangeCount = ch_avoid_indi->avoid_range_count;

   wlan_hdd_send_avoid_freq_event(hdd_ctxt, &hdd_avoid_freq_list);

   /* clear existing unsafe channel cache */
   hdd_ctxt->unsafe_channel_count = 0;
   vos_mem_zero(hdd_ctxt->unsafe_channel_list,
                                        sizeof(hdd_ctxt->unsafe_channel_list));

   for (range_loop = 0; range_loop < ch_avoid_indi->avoid_range_count;
                                                             range_loop++) {
       if (hdd_ctxt->unsafe_channel_count >= NUM_20MHZ_RF_CHANNELS) {
           hddLog(LOGW, FL("LTE Coex unsafe channel list full"));
           break;
       }

       start_channel = ieee80211_frequency_to_channel(
                        ch_avoid_indi->avoid_freq_range[range_loop].start_freq);
       end_channel   = ieee80211_frequency_to_channel(
                          ch_avoid_indi->avoid_freq_range[range_loop].end_freq);
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                    "%s : start %d : %d, end %d : %d", __func__,
                    ch_avoid_indi->avoid_freq_range[range_loop].start_freq,
                    start_channel,
                    ch_avoid_indi->avoid_freq_range[range_loop].end_freq,
                    end_channel);

       /* do not process frequency bands that are not mapped to predefined
        * channels
        */
       if (start_channel == 0 || end_channel == 0)
           continue;

       for (channel_loop = MIN_20MHZ_RF_CHANNEL; channel_loop <=
                                       MAX_20MHZ_RF_CHANNEL; channel_loop++) {
           if (rfChannels[channel_loop].targetFreq >=
                       ch_avoid_indi->avoid_freq_range[range_loop].start_freq) {
               start_channel_idx = channel_loop;
               break;
            }
       }
       for (channel_loop = MIN_20MHZ_RF_CHANNEL; channel_loop <=
                                       MAX_20MHZ_RF_CHANNEL; channel_loop++) {
           if (rfChannels[channel_loop].targetFreq >=
                       ch_avoid_indi->avoid_freq_range[range_loop].end_freq) {
               end_channel_idx = channel_loop;
               if (rfChannels[channel_loop].targetFreq >
                        ch_avoid_indi->avoid_freq_range[range_loop].end_freq)
                   end_channel_idx--;
               break;
            }
       }

       if (start_channel_idx == INVALID_RF_CHANNEL ||
                                         end_channel_idx == INVALID_RF_CHANNEL)
           continue;

       for (channel_loop = start_channel_idx; channel_loop <=
                                              end_channel_idx; channel_loop++) {
           hdd_ctxt->unsafe_channel_list[hdd_ctxt->unsafe_channel_count++]
                                      = rfChannels[channel_loop].channelNum;
           if (hdd_ctxt->unsafe_channel_count >= NUM_20MHZ_RF_CHANNELS) {
                hddLog(LOGW, FL("LTE Coex unsafe channel list full"));
                break;
           }
       }
   }

#ifdef CONFIG_CNSS
   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
            "%s : number of unsafe channels is %d ",
            __func__,  hdd_ctxt->unsafe_channel_count);

   if (vos_set_wlan_unsafe_channel(hdd_ctxt->unsafe_channel_list,
                                hdd_ctxt->unsafe_channel_count)) {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to set unsafe channel",
                __func__);

       /* clear existing unsafe channel cache */
       hdd_ctxt->unsafe_channel_count = 0;
       vos_mem_zero(hdd_ctxt->unsafe_channel_list,
           sizeof(v_U16_t) * NUM_20MHZ_RF_CHANNELS);

       return;
   }

   for (channel_loop = 0;
        channel_loop < hdd_ctxt->unsafe_channel_count;
        channel_loop++)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                 "%s: channel %d is not safe ", __func__,
                 hdd_ctxt->unsafe_channel_list[channel_loop]);
   }
#endif

    if (0 == hdd_ctxt->unsafe_channel_count)
       return;

    status = hdd_get_front_adapter(hdd_ctxt, &adapter_node);
    while (NULL != adapter_node && VOS_STATUS_SUCCESS == status) {
        hostapd_adapter = adapter_node->pAdapter;

        if( hostapd_adapter && (WLAN_HDD_SOFTAP ==
                               hostapd_adapter->device_mode)) {
            /* If auto channel select is enabled
             * preferred channel is in safe channel,
             * re-start softap interface with safe channel.
             * no overlap with preferred channel and safe channel
             * do not re-start softap interface
             * stay current operating channel.
             */
             if ((hostapd_adapter->sessionCtx.ap.sapConfig.acs_cfg.acs_mode) &&
                 (!hdd_find_prefd_safe_chnl(hdd_ctxt, hostapd_adapter)))
                 return;

            hddLog(LOG1, FL("Current operation channel %d"),
                      hostapd_adapter->sessionCtx.ap.operatingChannel);

            hddLog(LOG1, FL("sessionCtx.ap.sapConfig.channel %d"),
                      hostapd_adapter->sessionCtx.ap.sapConfig.channel);

            for (channel_loop = 0;
                 channel_loop < hdd_ctxt->unsafe_channel_count;
                 channel_loop++) {
                if (((hdd_ctxt->unsafe_channel_list[channel_loop] ==
                     hostapd_adapter->sessionCtx.ap.operatingChannel)) &&
                     (hostapd_adapter->sessionCtx.ap.sapConfig.acs_cfg.acs_mode
                                                                       == true)
                      && !restart_sap_in_progress) {

                    hddLog(LOG1, FL("Restarting SAP"));
                    wlan_hdd_send_svc_nlink_msg(WLAN_SVC_LTE_COEX_IND, NULL, 0);
                    restart_sap_in_progress = 1;
                    /* current operating channel is un-safe channel,
                     * restart driver
                     */
                    hdd_hostapd_stop(hostapd_adapter->dev);
                    return;
                }
            }
        }
        status = hdd_get_next_adapter (hdd_ctxt, adapter_node, &next);
        adapter_node = next;
     }
     return;
}
#endif /* FEATURE_WLAN_CH_AVOID */

#ifdef WLAN_FEATURE_LPSS
int wlan_hdd_gen_wlan_status_pack(struct wlan_status_data *data,
                                  hdd_adapter_t *pAdapter,
                                  hdd_station_ctx_t *pHddStaCtx,
                                  v_U8_t is_on,
                                  v_U8_t is_connected)
{
    hdd_context_t *pHddCtx = NULL;
    tANI_U8 buflen = WLAN_SVC_COUNTRY_CODE_LEN;

    if (!data) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: invalid data pointer", __func__);
        return (-1);
    }
    if (!pAdapter) {
        if (is_on) {
            /* no active interface */
            data->lpss_support = 0;
            data->is_on = is_on;
            return 0;
        }
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: invalid adapter pointer", __func__);
        return (-1);
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    if (pHddCtx->lpss_support && pHddCtx->cfg_ini->enablelpasssupport)
        data->lpss_support = 1;
    else
        data->lpss_support = 0;
    data->numChannels = WLAN_SVC_MAX_NUM_CHAN;
    sme_GetCfgValidChannels(pHddCtx->hHal, data->channel_list,
                            &data->numChannels);
    sme_GetCountryCode(pHddCtx->hHal, data->country_code, &buflen);
    data->is_on = is_on;
    data->vdev_id = pAdapter->sessionId;
    data->vdev_mode = pAdapter->device_mode;
    if (pHddStaCtx) {
        data->is_connected = is_connected;
        data->rssi = pAdapter->rssi;
        data->freq = vos_chan_to_freq(pHddStaCtx->conn_info.operationChannel);
        if (WLAN_SVC_MAX_SSID_LEN >= pHddStaCtx->conn_info.SSID.SSID.length) {
            data->ssid_len = pHddStaCtx->conn_info.SSID.SSID.length;
            memcpy(data->ssid,
                   pHddStaCtx->conn_info.SSID.SSID.ssId,
                   pHddStaCtx->conn_info.SSID.SSID.length);
        }
        if (WLAN_SVC_MAX_BSSID_LEN >= sizeof(pHddStaCtx->conn_info.bssId))
            memcpy(data->bssid,
                   pHddStaCtx->conn_info.bssId,
                   sizeof(pHddStaCtx->conn_info.bssId));
    }
    return 0;
}

int wlan_hdd_gen_wlan_version_pack(struct wlan_version_data *data,
                                    v_U32_t fw_version,
                                    v_U32_t chip_id,
                                    const char *chip_name)
{
    if (!data) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: invalid data pointer", __func__);
        return (-1);
    }

    data->chip_id = chip_id;
    strlcpy(data->chip_name, chip_name, WLAN_SVC_MAX_STR_LEN);
    if (strncmp(chip_name, "Unknown", 7))
        strlcpy(data->chip_from, "Qualcomm", WLAN_SVC_MAX_STR_LEN);
    else
        strlcpy(data->chip_from, "Unknown", WLAN_SVC_MAX_STR_LEN);
    strlcpy(data->host_version, QWLAN_VERSIONSTR, WLAN_SVC_MAX_STR_LEN);
    scnprintf(data->fw_version, WLAN_SVC_MAX_STR_LEN, "%d.%d.%d.%d",
              (fw_version & 0xf0000000) >> 28,
              (fw_version & 0xf000000) >> 24,
              (fw_version & 0xf00000) >> 20,
              (fw_version & 0x7fff));
    return 0;
}
#endif

#if defined(FEATURE_WLAN_LFR) && defined(WLAN_FEATURE_ROAM_SCAN_OFFLOAD)
/**---------------------------------------------------------------------------

  \brief wlan_hdd_disable_roaming()

  This function loop through each adapter and disable roaming on each STA
  device mode except the input adapter.
  Note: On the input adapter roaming is not enabled yet hence no need to
        disable.

  \param  - pAdapter HDD adapter pointer

  \return - None

  --------------------------------------------------------------------------*/
void wlan_hdd_disable_roaming(hdd_adapter_t *pAdapter)
{
    hdd_context_t           *pHddCtx      = WLAN_HDD_GET_CTX(pAdapter);
    hdd_adapter_t           *pAdapterIdx  = NULL;
    hdd_adapter_list_node_t *pAdapterNode = NULL;
    hdd_adapter_list_node_t *pNext        = NULL;
    VOS_STATUS status;

    if (pHddCtx->cfg_ini->isFastRoamIniFeatureEnabled &&
        pHddCtx->cfg_ini->isRoamOffloadScanEnabled &&
        WLAN_HDD_INFRA_STATION == pAdapter->device_mode &&
        vos_is_sta_active_connection_exists()) {
        hddLog(LOG1, FL("Connect received on STA sessionId(%d)"),
               pAdapter->sessionId);
        /* Loop through adapter and disable roaming for each STA device mode
           except the input adapter. */

        status = hdd_get_front_adapter (pHddCtx, &pAdapterNode);

        while (NULL != pAdapterNode && VOS_STATUS_SUCCESS == status) {
            pAdapterIdx = pAdapterNode->pAdapter;

            if (WLAN_HDD_INFRA_STATION == pAdapterIdx->device_mode &&
               pAdapter->sessionId != pAdapterIdx->sessionId) {
               hddLog(LOG1, FL("Disable Roaming on sessionId(%d)"),
                      pAdapterIdx->sessionId);
               sme_stopRoaming(WLAN_HDD_GET_HAL_CTX(pAdapterIdx),
                               pAdapterIdx->sessionId, 0);
            }

            status = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pNext);
            pAdapterNode = pNext;
        }
    }
}

/**---------------------------------------------------------------------------

  \brief wlan_hdd_enable_roaming()

  This function loop through each adapter and enable roaming on each STA
  device mode except the input adapter.
  Note: On the input adapter no need to enable roaming because link got
        disconnected on this.

  \param  - pAdapter HDD adapter pointer

  \return - None

  --------------------------------------------------------------------------*/
void wlan_hdd_enable_roaming(hdd_adapter_t *pAdapter)
{
    hdd_context_t           *pHddCtx      = WLAN_HDD_GET_CTX(pAdapter);
    hdd_adapter_t           *pAdapterIdx  = NULL;
    hdd_adapter_list_node_t *pAdapterNode = NULL;
    hdd_adapter_list_node_t *pNext        = NULL;
    VOS_STATUS status;

    if (pHddCtx->cfg_ini->isFastRoamIniFeatureEnabled &&
        pHddCtx->cfg_ini->isRoamOffloadScanEnabled &&
        WLAN_HDD_INFRA_STATION == pAdapter->device_mode &&
        vos_is_sta_active_connection_exists()) {
        hddLog(LOG1, FL("Disconnect received on STA sessionId(%d)"),
               pAdapter->sessionId);
        /* Loop through adapter and enable roaming for each STA device mode
           except the input adapter. */

        status = hdd_get_front_adapter (pHddCtx, &pAdapterNode);

        while (NULL != pAdapterNode && VOS_STATUS_SUCCESS == status) {
            pAdapterIdx = pAdapterNode->pAdapter;

            if (WLAN_HDD_INFRA_STATION == pAdapterIdx->device_mode &&
               pAdapter->sessionId != pAdapterIdx->sessionId) {
               hddLog(LOG1, FL("Enabling Roaming on sessionId(%d)"),
                      pAdapterIdx->sessionId);
               sme_startRoaming(WLAN_HDD_GET_HAL_CTX(pAdapterIdx),
                               pAdapterIdx->sessionId,
                               REASON_CONNECT);
            }

            status = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pNext);
            pAdapterNode = pNext;
        }
    }
}
#endif


void wlan_hdd_send_svc_nlink_msg(int type, void *data, int len)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    tAniMsgHdr *ani_hdr;
    void *nl_data = NULL;
    int flags = GFP_KERNEL;

    if (in_interrupt() || irqs_disabled() || in_atomic())
        flags = GFP_ATOMIC;

    skb = alloc_skb(NLMSG_SPACE(WLAN_NL_MAX_PAYLOAD), flags);

    if(skb == NULL) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: alloc_skb failed", __func__);
        return;
    }

    nlh = (struct nlmsghdr *)skb->data;
    nlh->nlmsg_pid = 0;  /* from kernel */
    nlh->nlmsg_flags = 0;
    nlh->nlmsg_seq = 0;
    nlh->nlmsg_type = WLAN_NL_MSG_SVC;

    ani_hdr = NLMSG_DATA(nlh);
    ani_hdr->type = type;

    switch(type) {
    case WLAN_SVC_FW_CRASHED_IND:
    case WLAN_SVC_LTE_COEX_IND:
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
    case WLAN_SVC_WLAN_AUTO_SHUTDOWN_IND:
#endif
        ani_hdr->length = 0;
        nlh->nlmsg_len = NLMSG_LENGTH((sizeof(tAniMsgHdr)));
        skb_put(skb, NLMSG_SPACE(sizeof(tAniMsgHdr)));
        break;
    case WLAN_SVC_WLAN_STATUS_IND:
    case WLAN_SVC_WLAN_VERSION_IND:
    case WLAN_SVC_DFS_CAC_START_IND:
    case WLAN_SVC_DFS_CAC_END_IND:
    case WLAN_SVC_DFS_RADAR_DETECT_IND:
    case WLAN_SVC_DFS_ALL_CHANNEL_UNAVAIL_IND:
    case WLAN_SVC_WLAN_TP_IND:
    case WLAN_SVC_WLAN_TP_TX_IND:
    case WLAN_SVC_RPS_ENABLE_IND:
        ani_hdr->length = len;
        nlh->nlmsg_len = NLMSG_LENGTH((sizeof(tAniMsgHdr) + len));
        nl_data = (char *)ani_hdr + sizeof(tAniMsgHdr);
        memcpy(nl_data, data, len);
        skb_put(skb, NLMSG_SPACE(sizeof(tAniMsgHdr) + len));
        break;

    default:
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "WLAN SVC: Attempt to send unknown nlink message %d", type);
        kfree_skb(skb);
        return;
    }

    nl_srv_bcast(skb);

    return;
}

#ifdef WLAN_FEATURE_LPSS
void wlan_hdd_send_status_pkg(hdd_adapter_t *pAdapter,
                              hdd_station_ctx_t *pHddStaCtx,
                              v_U8_t is_on,
                              v_U8_t is_connected)
{
    int ret = 0;
    struct wlan_status_data data;

    if (VOS_FTM_MODE == hdd_get_conparam())
        return;

    memset(&data, 0, sizeof(struct wlan_status_data));
    if (is_on)
        ret = wlan_hdd_gen_wlan_status_pack(&data, pAdapter, pHddStaCtx,
                                            is_on, is_connected);
    if (!ret)
        wlan_hdd_send_svc_nlink_msg(WLAN_SVC_WLAN_STATUS_IND,
                                    &data, sizeof(struct wlan_status_data));
}

void wlan_hdd_send_version_pkg(v_U32_t fw_version,
                               v_U32_t chip_id,
                               const char *chip_name)
{
    int ret = 0;
    struct wlan_version_data data;

    if (VOS_FTM_MODE == hdd_get_conparam())
        return;

    memset(&data, 0, sizeof(struct wlan_version_data));
    ret = wlan_hdd_gen_wlan_version_pack(&data, fw_version, chip_id, chip_name);
    if (!ret)
        wlan_hdd_send_svc_nlink_msg(WLAN_SVC_WLAN_VERSION_IND,
                                    &data, sizeof(struct wlan_version_data));
}

void wlan_hdd_send_all_scan_intf_info(hdd_context_t *pHddCtx)
{
    hdd_adapter_t *pDataAdapter = NULL;
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
    v_BOOL_t scan_intf_found = VOS_FALSE;
    VOS_STATUS status;

    if (!pHddCtx) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: NULL pointer for pHddCtx",
                  __func__);
        return;
    }

   status = hdd_get_front_adapter(pHddCtx, &pAdapterNode);
   while (NULL != pAdapterNode && VOS_STATUS_SUCCESS == status) {
       pDataAdapter = pAdapterNode->pAdapter;
       if (pDataAdapter) {
           if (pDataAdapter->device_mode == WLAN_HDD_INFRA_STATION ||
               pDataAdapter->device_mode == WLAN_HDD_P2P_CLIENT ||
               pDataAdapter->device_mode == WLAN_HDD_P2P_DEVICE) {
               scan_intf_found = VOS_TRUE;
               wlan_hdd_send_status_pkg(pDataAdapter, NULL, 1, 0);
           }
       }
       status = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pNext);
       pAdapterNode = pNext;
   }

   if (!scan_intf_found)
       wlan_hdd_send_status_pkg(pDataAdapter, NULL, 1, 0);
}
#endif

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
v_VOID_t wlan_hdd_auto_shutdown_cb(v_VOID_t)
{
    hddLog(LOGE, FL("%s: Wlan Idle. Sending Shutdown event.."),__func__);
    wlan_hdd_send_svc_nlink_msg(WLAN_SVC_WLAN_AUTO_SHUTDOWN_IND, NULL, 0);
}

void wlan_hdd_auto_shutdown_enable(hdd_context_t *hdd_ctx, v_BOOL_t enable)
{
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
    VOS_STATUS status;
    hdd_adapter_t      *pAdapter;
    v_BOOL_t ap_connected = VOS_FALSE, sta_connected = VOS_FALSE;
    tHalHandle hHal;

    hHal = hdd_ctx->hHal;
    if (hHal == NULL)
        return;

    if (hdd_ctx->cfg_ini->WlanAutoShutdown == 0)
        return;

    if (enable == VOS_FALSE) {
        if (sme_set_auto_shutdown_timer(hHal, 0) != eHAL_STATUS_SUCCESS) {
               hddLog(LOGE, FL("Failed to stop wlan auto shutdown timer"));
        }
        return;
    }

    /* To enable shutdown timer check concurrency */
    if (vos_concurrent_open_sessions_running()) {
        status = hdd_get_front_adapter ( hdd_ctx, &pAdapterNode );

        while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status ) {
            pAdapter = pAdapterNode->pAdapter;
            if (pAdapter && pAdapter->device_mode == WLAN_HDD_INFRA_STATION) {
                if (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter)->conn_info.connState
                                               == eConnectionState_Associated) {
                    sta_connected = VOS_TRUE;
                    break;
                }
            }
            if (pAdapter && pAdapter->device_mode == WLAN_HDD_SOFTAP) {
                if(WLAN_HDD_GET_AP_CTX_PTR(pAdapter)->bApActive == VOS_TRUE) {
                    ap_connected = VOS_TRUE;
                    break;
                }
            }
            status = hdd_get_next_adapter ( hdd_ctx, pAdapterNode, &pNext );
            pAdapterNode = pNext;
        }
    }

    if (ap_connected == VOS_TRUE || sta_connected == VOS_TRUE) {
            hddLog(LOG1, FL("CC Session active. Shutdown timer not enabled"));
            return;
    } else {
        if (sme_set_auto_shutdown_timer(hHal,
                hdd_ctx->cfg_ini->WlanAutoShutdown)
                != eHAL_STATUS_SUCCESS)
           hddLog(LOGE, FL("Failed to start wlan auto shutdown timer"));
        else
           hddLog(LOG1, FL("Auto Shutdown timer for %d seconds enabled"),
                                   hdd_ctx->cfg_ini->WlanAutoShutdown);

    }
}
#endif

hdd_adapter_t * hdd_get_con_sap_adapter(hdd_adapter_t *this_sap_adapter,
                                        bool check_start_bss)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(this_sap_adapter);
    hdd_adapter_t *pAdapter, *con_sap_adapter;
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;

    con_sap_adapter = NULL;

    status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
    while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status ) {
        pAdapter = pAdapterNode->pAdapter;
        if (pAdapter && ((pAdapter->device_mode == WLAN_HDD_SOFTAP) ||
            (pAdapter->device_mode == WLAN_HDD_P2P_GO)) &&
            pAdapter != this_sap_adapter) {
            if (check_start_bss) {
                if (test_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags)) {
                    con_sap_adapter = pAdapter;
                    break;
                }
            } else {
                    con_sap_adapter = pAdapter;
                    break;
            }
        }
        status = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pNext );
        pAdapterNode = pNext;
    }

    return con_sap_adapter;
}

#ifdef FEATURE_BUS_BANDWIDTH
void hdd_start_bus_bw_compute_timer(hdd_adapter_t *pAdapter)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if (VOS_TIMER_STATE_RUNNING ==
        vos_timer_getCurrentState(&pHddCtx->bus_bw_timer))
        return;

    vos_timer_start(&pHddCtx->bus_bw_timer,
            pHddCtx->cfg_ini->busBandwidthComputeInterval);
}

void hdd_stop_bus_bw_compute_timer(hdd_adapter_t *pAdapter)
{
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
    VOS_STATUS status;
    v_BOOL_t can_stop = VOS_TRUE;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if (VOS_TIMER_STATE_RUNNING !=
        vos_timer_getCurrentState(&pHddCtx->bus_bw_timer)) {
        /* trying to stop timer, when not running is not good */
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "bus band width compute timer is not running");
        return;
    }

    if (vos_concurrent_open_sessions_running()) {
        status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

        while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status ) {
            pAdapter = pAdapterNode->pAdapter;
            if (pAdapter && (pAdapter->device_mode == WLAN_HDD_INFRA_STATION ||
                        pAdapter->device_mode == WLAN_HDD_P2P_CLIENT) &&
                    WLAN_HDD_GET_STATION_CTX_PTR(pAdapter)->conn_info.connState
                    == eConnectionState_Associated) {
                can_stop = VOS_FALSE;
                break;
            }
            if (pAdapter && (pAdapter->device_mode == WLAN_HDD_SOFTAP ||
                        pAdapter->device_mode == WLAN_HDD_P2P_GO) &&
                    WLAN_HDD_GET_AP_CTX_PTR(pAdapter)->bApActive == VOS_TRUE) {
                can_stop = VOS_FALSE;
                break;
            }
            status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
            pAdapterNode = pNext;
        }
    }

    if(can_stop == VOS_TRUE)
        vos_timer_stop(&pHddCtx->bus_bw_timer);
}
#endif


#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
void wlan_hdd_check_sta_ap_concurrent_ch_intf(void *data)
{
    hdd_adapter_t *ap_adapter = NULL, *sta_adapter = (hdd_adapter_t *)data;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(sta_adapter);
    tHalHandle hHal;
    hdd_ap_ctx_t *pHddApCtx;
    uint16_t intf_ch = 0, vht_channel_width = 0;

   if ((pHddCtx->cfg_ini->WlanMccToSccSwitchMode == VOS_MCC_TO_SCC_SWITCH_DISABLE)
       || !(vos_concurrent_open_sessions_running()
       || !(vos_get_concurrency_mode() == VOS_STA_SAP)))
        return;

    ap_adapter = hdd_get_adapter(pHddCtx, WLAN_HDD_SOFTAP);
    if (ap_adapter == NULL)
        return;

    if (!test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags))
        return;

    pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
    hHal = WLAN_HDD_GET_HAL_CTX(ap_adapter);

    if (hHal == NULL)
        return;

#ifdef WLAN_FEATURE_MBSSID
    intf_ch = WLANSAP_CheckCCIntf(pHddApCtx->sapContext);
    vht_channel_width = wlan_sap_get_vht_ch_width(pHddApCtx->sapContext);
#else
    intf_ch = WLANSAP_CheckCCIntf(pHddCtx->pvosContext);
    vht_channel_width = wlan_sap_get_vht_ch_width(pHddApCtx->pvosContext);
#endif
    if (intf_ch == 0)
        return;

    pHddApCtx->sapConfig.channel = intf_ch;
    sme_SelectCBMode(hHal,
                     pHddApCtx->sapConfig.SapHw_mode,
                     pHddApCtx->sapConfig.channel,
                     pHddApCtx->sapConfig.sec_ch,
                     &vht_channel_width, pHddApCtx->sapConfig.ch_width_orig);
#ifdef WLAN_FEATURE_MBSSID
    wlan_sap_set_vht_ch_width(pHddApCtx->sapContext, vht_channel_width);
#else
    wlan_sap_set_vht_ch_width(pHddApCtx->pvosContext, vht_channel_width);
#endif
    wlan_hdd_restart_sap(ap_adapter);
}
#endif

/**
 * wlan_hdd_check_custom_con_channel_rules() - This function checks the sap's
 *                                             and sta's operating channel.
 * @sta_adapter:  Describe the first argument to foobar.
 * @ap_adapter:   Describe the second argument to foobar.
 * @roam_profile: Roam profile of AP to which STA wants to connect.
 * @concurrent_chnl_same: If both SAP and STA channels are same then
 *                        set this flag to true else false.
 *
 * This function checks the sap's operating channel and sta's operating channel.
 * if both are same then it will return false else it will restart the sap in
 * sta's channel and return true.
 *
 *
 * Return: VOS_STATUS_SUCCESS or VOS_STATUS_E_FAILURE.
 */
VOS_STATUS wlan_hdd_check_custom_con_channel_rules(hdd_adapter_t *sta_adapter,
                                                  hdd_adapter_t *ap_adapter,
                                                  tCsrRoamProfile *roam_profile,
                                                  tScanResultHandle *scan_cache,
                                                  bool *concurrent_chnl_same)
{
    hdd_ap_ctx_t *hdd_ap_ctx;
    uint8_t channel_id;
    VOS_STATUS status;
    device_mode_t device_mode = ap_adapter->device_mode;
    *concurrent_chnl_same = true;

    hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
    status =
     sme_get_ap_channel_from_scan_cache(WLAN_HDD_GET_HAL_CTX(sta_adapter),
                                        roam_profile,
                                        scan_cache,
                                        &channel_id);
    if ((VOS_STATUS_SUCCESS == status)) {
        if ((WLAN_HDD_SOFTAP == device_mode) &&
            (channel_id < SIR_11A_CHANNEL_BEGIN)) {
             if (hdd_ap_ctx->operatingChannel != channel_id) {
                 *concurrent_chnl_same = false;
                  hddLog(VOS_TRACE_LEVEL_INFO_MED,
                            FL("channels are different"));
             }
        } else if ((WLAN_HDD_P2P_GO == device_mode) &&
                   (channel_id >= SIR_11A_CHANNEL_BEGIN)) {
             if (hdd_ap_ctx->operatingChannel != channel_id) {
                 *concurrent_chnl_same = false;
                 hddLog(VOS_TRACE_LEVEL_INFO_MED,
                           FL("channels are different"));
             }
        }
    } else {
        /*
         * Lets handle worst case scenario here, Scan cache lookup is failed
         * so we have to stop the SAP to avoid any channel discrepancy  between
         * SAP's channel and STA's channel. Return the status as failure so
         * caller function could know that scan look up is failed.
         */
        hddLog(VOS_TRACE_LEVEL_ERROR,
                    FL("Finding AP from scan cache failed"));
        return VOS_STATUS_E_FAILURE;
    }
    return VOS_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_MBSSID
/**
 * wlan_hdd_stop_sap() - This function stops bss of SAP.
 * @ap_adapter: SAP adapter
 *
 * This function will process the stopping of sap adapter.
 *
 * Return: void.
 */
void wlan_hdd_stop_sap(hdd_adapter_t *ap_adapter)
{
    hdd_ap_ctx_t *hdd_ap_ctx;
    hdd_hostapd_state_t *hostapd_state;
    VOS_STATUS vos_status;
    hdd_context_t *hdd_ctx;
#ifdef CFG80211_DEL_STA_V2
    struct station_del_parameters delStaParams;
#endif

    if (NULL == ap_adapter) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    FL("ap_adapter is NULL here"));
        return;
    }

    hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
    hdd_ctx = WLAN_HDD_GET_CTX(ap_adapter);
    if (0 != wlan_hdd_validate_context(hdd_ctx))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return;
    }
    mutex_lock(&hdd_ctx->sap_lock);
    if (test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags)) {
#ifdef CFG80211_DEL_STA_V2
        delStaParams.mac = NULL;
        delStaParams.subtype = SIR_MAC_MGMT_DEAUTH >> 4;
        delStaParams.reason_code = eCsrForcedDeauthSta;
        wlan_hdd_cfg80211_del_station(ap_adapter->wdev.wiphy, ap_adapter->dev,
                                      &delStaParams);
#else
        wlan_hdd_cfg80211_del_station(ap_adapter->wdev.wiphy, ap_adapter->dev,
                                      NULL);
#endif
        hdd_cleanup_actionframe(hdd_ctx, ap_adapter);
        hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(ap_adapter);
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
                  FL("Now doing SAP STOPBSS"));
        if (VOS_STATUS_SUCCESS == WLANSAP_StopBss(hdd_ap_ctx->sapContext)) {
            vos_status = vos_wait_single_event(&hostapd_state->stop_bss_event,
                                               10000);
            if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
                mutex_unlock(&hdd_ctx->sap_lock);
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                          FL("SAP Stop Failed"));
                return;
            }
        }
        clear_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags);
        wlan_hdd_decr_active_session(hdd_ctx, ap_adapter->device_mode);
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
                  FL("SAP Stop Success"));
    } else {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("Can't stop ap because its not started"));
    }
    mutex_unlock(&hdd_ctx->sap_lock);
    return;
}

/**
 * wlan_hdd_start_sap() - This function starts bss of SAP.
 * @ap_adapter: SAP adapter
 *
 * This function will process the starting of sap adapter.
 *
 * Return: void.
 */
void wlan_hdd_start_sap(hdd_adapter_t *ap_adapter)
{
    hdd_ap_ctx_t *hdd_ap_ctx;
    hdd_hostapd_state_t *hostapd_state;
    VOS_STATUS vos_status;
    hdd_context_t *hdd_ctx;
    tsap_Config_t *pConfig;

    if (NULL == ap_adapter) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    FL("ap_adapter is NULL here"));
        return;
    }

    hdd_ctx = WLAN_HDD_GET_CTX(ap_adapter);
    hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
    hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(ap_adapter);
    pConfig = &ap_adapter->sessionCtx.ap.sapConfig;

    if (0 != wlan_hdd_validate_context(hdd_ctx))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return;
    }
    mutex_lock(&hdd_ctx->sap_lock);
    if (test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags))
        goto end;

    if (0 != wlan_hdd_cfg80211_update_apies(ap_adapter)) {
        hddLog(LOGE, FL("SAP Not able to set AP IEs"));
        WLANSAP_ResetSapConfigAddIE(pConfig, eUPDATE_IE_ALL);
        goto end;
    }

    if (WLANSAP_StartBss(hdd_ap_ctx->sapContext, hdd_hostapd_SAPEventCB,
                         &hdd_ap_ctx->sapConfig, (v_PVOID_t)ap_adapter->dev)
                         != VOS_STATUS_SUCCESS) {
        goto end;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
              FL("Waiting for SAP to start"));
    vos_status = vos_wait_single_event(&hostapd_state->vosEvent, 10000);
    if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("SAP Start failed"));
        goto end;
    }
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
              FL("SAP Start Success"));
    set_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags);
    wlan_hdd_incr_active_session(hdd_ctx, ap_adapter->device_mode);
    hostapd_state->bCommit = TRUE;

end:
    mutex_unlock(&hdd_ctx->sap_lock);
    return;
}
#endif

/**
 * hdd_wlan_go_set_mcc_p2p_quota() - Function to set quota for P2P GO
 * @hostapd_adapter:	Pointer to HDD adapter
 * @set_value:		Qouta value for the interface
 *
 * This function is used to set the quota for P2P GO cases
 *
 * Return: Configuration message posting status, SUCCESS or Fail
 *
 */
int32_t hdd_wlan_go_set_mcc_p2p_quota(hdd_adapter_t *hostapd_adapter,
					uint32_t set_value)
{
	uint8_t first_adapter_operating_channel = 0;
	uint8_t second_adapter_opertaing_channel = 0;
	tVOS_CONCURRENCY_MODE concurrent_state = 0;
	hdd_adapter_t *staAdapter = NULL;
	int32_t ret = 0; /* success */

	/*
	 * Check if concurrency mode is active.
	 * Need to modify this code to support MCC modes other than
	 * STA/P2P GO
	 */

	concurrent_state = hdd_get_concurrency_mode();
	if (concurrent_state == (VOS_STA | VOS_P2P_GO)) {
		hddLog(LOG1, "%s: STA & P2P are both enabled", __func__);

		/*
		 * The channel numbers for both adapters and the time
		 * quota for the 1st adapter, i.e., one specified in cmd
		 * are formatted as a bit vector then passed on to WMA
		 * +************************************************+
		 * |bit 31-24 |bit 23-16  |  bits 15-8  |bits 7-0   |
		 * |  Unused  |  Quota for| chan. # for |chan. # for|
		 * |          |  1st chan.| 1st chan.   |2nd chan.  |
		 * +************************************************+
		 */

		/* Get the operating channel of the specified vdev */
		first_adapter_operating_channel =
			hdd_get_operating_channel(hostapd_adapter->pHddCtx,
			hostapd_adapter->device_mode);

		hddLog(LOG1, "%s: 1st channel No.:%d and quota:%dms",
			__func__, first_adapter_operating_channel,
			set_value);

		/* Move the time quota for first adapter to bits 15-8 */
		set_value = set_value << 8;
		/*
		 * Store the operating channel number of 1st adapter at
		 * the lower 8-bits of bit vector.
		 */
		set_value = set_value | first_adapter_operating_channel;
		if (hostapd_adapter->device_mode ==
				WLAN_HDD_INFRA_STATION) {
			/* iwpriv cmd issued on wlan0; get p2p0 vdev chan */
			if ((concurrent_state & VOS_P2P_CLIENT) != 0) {
				/* The 2nd MCC vdev is P2P client */
				staAdapter = hdd_get_adapter
					(
					 hostapd_adapter->pHddCtx,
					 WLAN_HDD_P2P_CLIENT
					);
			} else {
				/* The 2nd MCC vdev is P2P GO */
				staAdapter = hdd_get_adapter
					(
					 hostapd_adapter->pHddCtx,
					 WLAN_HDD_P2P_GO
					);
			}
		} else {
			/* iwpriv cmd issued on p2p0; get channel for wlan0 */
			staAdapter = hdd_get_adapter
				(
				 hostapd_adapter->pHddCtx,
				 WLAN_HDD_INFRA_STATION
				);
		}
		if (staAdapter != NULL) {
			second_adapter_opertaing_channel =
				hdd_get_operating_channel
				(
				 staAdapter->pHddCtx,
				 staAdapter->device_mode
				);
			hddLog(LOG1, "%s: 2nd vdev channel No. is:%d",
					__func__,
					second_adapter_opertaing_channel);

			if (second_adapter_opertaing_channel == 0 ||
					first_adapter_operating_channel == 0) {
				hddLog(LOGE, "Invalid channel");
				return -EINVAL;
			}

			/*
			 * Move the time quota and operating channel number
			 * for the first adapter to bits 23-16 & bits 15-8
			 * of set_value vector, respectively.
			 */
			set_value = set_value << 8;
			/*
			 * Store the channel number for 2nd MCC vdev at bits
			 * 7-0 of set_value vector as per the bit format above.
			 */
			set_value = set_value |
				second_adapter_opertaing_channel;
			ret = process_wma_set_command
				(
				 (int32_t)hostapd_adapter->sessionId,
				 (int32_t)WMA_VDEV_MCC_SET_TIME_QUOTA,
				 set_value,
				 VDEV_CMD
				);
		} else {
			hddLog(LOGE, "%s: NULL adapter handle. Exit",
					__func__);
		}
	} else {
		hddLog(LOG1, "%s: MCC is not active. "
				"Exit w/o setting latency", __func__);
	}
	return ret;
}

/**
 * hdd_wlan_set_mcc_p2p_quota() - Function to set quota for P2P
 * @hostapd_adapter:    Pointer to HDD adapter
 * @set_value:          Qouta value for the interface
 *
 * This function is used to set the quota for P2P cases
 *
 * Return: Configuration message posting status, SUCCESS or Fail
 *
 */
int32_t hdd_wlan_set_mcc_p2p_quota(hdd_adapter_t *hostapd_adapater,
					uint32_t set_value)
{
	uint8_t first_adapter_operating_channel = 0;
	uint8_t second_adapter_opertaing_channel = 0;
	hdd_adapter_t *staAdapter = NULL;
	int32_t ret = 0; /* success */

	tVOS_CONCURRENCY_MODE concurrent_state = hdd_get_concurrency_mode();
	hddLog(LOG1, "iwpriv cmd to set MCC quota with val %dms",
			set_value);
	/*
	 * Check if concurrency mode is active.
	 * Need to modify this code to support MCC modes other than STA/P2P
	 */
	if ((concurrent_state == (VOS_STA | VOS_P2P_CLIENT)) ||
			(concurrent_state == (VOS_STA | VOS_P2P_GO))) {
		hddLog(LOG1, "STA & P2P are both enabled");
		/*
		 * The channel numbers for both adapters and the time
		 * quota for the 1st adapter, i.e., one specified in cmd
		 * are formatted as a bit vector then passed on to WMA
		 * +***********************************************************+
		 * |bit 31-24  | bit 23-16  |   bits 15-8   |   bits 7-0       |
		 * |  Unused   | Quota for  | chan. # for   |   chan. # for    |
		 * |           | 1st chan.  | 1st chan.     |   2nd chan.      |
		 * +***********************************************************+
		 */
		/* Get the operating channel of the specified vdev */
		first_adapter_operating_channel =
			hdd_get_operating_channel
			(
			 hostapd_adapater->pHddCtx,
			 hostapd_adapater->device_mode
			);
		hddLog(LOG1, "1st channel No.:%d and quota:%dms",
				first_adapter_operating_channel, set_value);
		/* Move the time quota for first channel to bits 15-8 */
		set_value = set_value << 8;
		/*
		 * Store the channel number of 1st channel at bits 7-0
		 * of the bit vector
		 */
		set_value = set_value | first_adapter_operating_channel;
		/* Find out the 2nd MCC adapter and its operating channel */
		if (hostapd_adapater->device_mode == WLAN_HDD_INFRA_STATION) {
			/*
			 * iwpriv cmd was issued on wlan0;
			 * get p2p0 vdev channel
			 */
			if ((concurrent_state & VOS_P2P_CLIENT) != 0) {
				/* The 2nd MCC vdev is P2P client */
				staAdapter = hdd_get_adapter(
						hostapd_adapater->pHddCtx,
						WLAN_HDD_P2P_CLIENT);
			} else {
				/* The 2nd MCC vdev is P2P GO */
				staAdapter = hdd_get_adapter(
						hostapd_adapater->pHddCtx,
						WLAN_HDD_P2P_GO);
			}
		} else {
			/*
			 * iwpriv cmd was issued on p2p0;
			 * get wlan0 vdev channel
			 */
			staAdapter = hdd_get_adapter(hostapd_adapater->pHddCtx,
					WLAN_HDD_INFRA_STATION);
		}
		if (staAdapter != NULL) {
			second_adapter_opertaing_channel =
				hdd_get_operating_channel
				(
				 staAdapter->pHddCtx,
				 staAdapter->device_mode
				);
			hddLog(LOG1, "2nd vdev channel No. is:%d",
					second_adapter_opertaing_channel);

			if (second_adapter_opertaing_channel == 0 ||
					first_adapter_operating_channel == 0) {
				hddLog(LOGE, "Invalid channel");
				return -EINVAL;
			}
			/*
			 * Now move the time quota and channel number of the
			 * 1st adapter to bits 23-16 and bits 15-8 of the bit
			 * vector, respectively.
			 */
			set_value = set_value << 8;
			/*
			 * Store the channel number for 2nd MCC vdev at bits
			 * 7-0 of set_value
			 */
			set_value = set_value |
					second_adapter_opertaing_channel;
			ret = process_wma_set_command(
					(int32_t)hostapd_adapater->sessionId,
					(int32_t)WMA_VDEV_MCC_SET_TIME_QUOTA,
					set_value, VDEV_CMD);
		} else {
			hddLog(LOGE, "NULL adapter handle. Exit");
		}
	} else {
		hddLog(LOG1, "%s: MCC is not active. Exit w/o setting latency",
				__func__);
	}
	return ret;
}

/**
 * hdd_get_fw_version() - Get FW version
 * @hdd_ctx:     pointer to HDD context.
 * @major_spid:  FW version - major spid.
 * @minor_spid:  FW version - minor spid
 * @ssid:        FW version - ssid
 * @crmid:       FW version - crmid
 *
 * This function is called to get the firmware build version stored
 * as part of the HDD context
 *
 * Return:   None
 */

void hdd_get_fw_version(hdd_context_t *hdd_ctx,
			uint32_t *major_spid, uint32_t *minor_spid,
			uint32_t *siid, uint32_t *crmid)
{
	*major_spid = (hdd_ctx->target_fw_version & 0xf0000000) >> 28;
	*minor_spid = (hdd_ctx->target_fw_version & 0xf000000) >> 24;
	*siid = (hdd_ctx->target_fw_version & 0xf00000) >> 20;
	*crmid = hdd_ctx->target_fw_version & 0x7fff;
}

/**
 * hdd_is_memdump_supported() - to check if memdump feature support
 *
 * This function is used to check if memdump feature is supported in
 * the host driver
 *
 * Return: true if supported and false otherwise
 */
bool hdd_is_memdump_supported(void)
{
#ifdef WLAN_FEATURE_MEMDUMP
	return true;
#endif
	return false;
}

#ifdef QCA_CONFIG_SMP
int wlan_hdd_get_cpu()
{
	int cpu_index = get_cpu();
	put_cpu();
	return cpu_index;
}
#endif

/**
 * hdd_get_fwpath() - get framework path
 *
 * This function is used to get the string written by
 * userspace to start the wlan driver
 *
 * Return: string
 */
const char *hdd_get_fwpath(void)
{
	return fwpath.string;
}

//Register the module init/exit functions
module_init(hdd_module_init);
module_exit(hdd_module_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Qualcomm Atheros, Inc.");
MODULE_DESCRIPTION("WLAN HOST DEVICE DRIVER");

#if  defined(QCA_WIFI_FTM)
module_param(con_mode, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#else
module_param_call(con_mode, con_mode_handler, param_get_int, &con_mode,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#endif

module_param_call(fwpath, fwpath_changed_handler, param_get_string, &fwpath,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

module_param(enable_dfs_chan_scan, int,
             S_IRUSR | S_IRGRP | S_IROTH);

module_param(enable_11d, int,
             S_IRUSR | S_IRGRP | S_IROTH);

module_param(country_code, charp,
             S_IRUSR | S_IRGRP | S_IROTH);
