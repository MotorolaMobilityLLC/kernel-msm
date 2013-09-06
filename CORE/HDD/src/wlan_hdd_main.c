/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

/*========================================================================

  \file  wlan_hdd_main.c

  \brief WLAN Host Device Driver implementation

   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.

   Qualcomm Confidential and Proprietary.

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
//#include <wlan_qct_driver.h>
#include <wlan_hdd_includes.h>
#include <vos_api.h>
#include <vos_sched.h>
#include <vos_power.h>
#include <linux/etherdevice.h>
#include <linux/firmware.h>
#ifdef ANI_BUS_TYPE_PLATFORM
#include <linux/wcnss_wlan.h>
#endif //ANI_BUS_TYPE_PLATFORM
#ifdef ANI_BUS_TYPE_PCI
#include "wcnss_wlan.h"
#endif /* ANI_BUS_TYPE_PCI */
#include <wlan_hdd_tx_rx.h>
#include <palTimer.h>
#include <wniApi.h>
#include <wlan_nlink_srv.h>
#include <wlan_btc_svc.h>
#include <wlan_hdd_cfg.h>
#include <wlan_ptt_sock_svc.h>
#include <wlan_hdd_wowl.h>
#include <wlan_hdd_misc.h>
#include <wlan_hdd_wext.h>
#ifdef WLAN_BTAMP_FEATURE
#include <bap_hdd_main.h>
#include <bapInternal.h>
#endif // WLAN_BTAMP_FEATURE

#include <linux/wireless.h>
#include <net/cfg80211.h>
#include "wlan_hdd_cfg80211.h"
#include "wlan_hdd_p2p.h"
#include <linux/rtnetlink.h>
int wlan_hdd_ftm_start(hdd_context_t *pAdapter);
#include "sapApi.h"
#include <linux/semaphore.h>
#include <mach/subsystem_restart.h>
#include <wlan_hdd_hostapd.h>
#include <wlan_hdd_softap_tx_rx.h>
#include "cfgApi.h"
#include "wlan_hdd_dev_pwr.h"
#ifdef WLAN_BTAMP_FEATURE
#include "bap_hdd_misc.h"
#endif
#include "wlan_qct_pal_trace.h"
#include "qwlan_version.h"
#include "wlan_qct_wda.h"
#ifdef FEATURE_WLAN_TDLS
#include "wlan_hdd_tdls.h"
#endif
#include "wlan_hdd_debugfs.h"

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

/* the Android framework expects this param even though we don't use it */
#define BUF_LEN 20
static char fwpath_buffer[BUF_LEN];
static struct kparam_string fwpath = {
   .string = fwpath_buffer,
   .maxlen = BUF_LEN,
};
#ifndef MODULE
static int wlan_hdd_inited;
#endif

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
 * Driver miracast parameters 0-Disabled
 * 1-Source, 2-Sink
 */
#define WLAN_HDD_DRIVER_MIRACAST_CFG_MIN_VAL 0
#define WLAN_HDD_DRIVER_MIRACAST_CFG_MAX_VAL 2

#ifdef WLAN_OPEN_SOURCE
static struct wake_lock wlan_wake_lock;
#endif
/* set when SSR is needed after unload */
static e_hdd_ssr_required isSsrRequired = HDD_SSR_NOT_REQUIRED;

//internal function declaration
static VOS_STATUS wlan_hdd_framework_restart(hdd_context_t *pHddCtx);
static void wlan_hdd_restart_init(hdd_context_t *pHddCtx);
static void wlan_hdd_restart_deinit(hdd_context_t *pHddCtx);
void wlan_hdd_restart_timer_cb(v_PVOID_t usrDataForCallback);
void hdd_set_wlan_suspend_mode(bool suspend);

v_U16_t hdd_select_queue(struct net_device *dev,
    struct sk_buff *skb);

#ifdef WLAN_FEATURE_PACKET_FILTERING
static void hdd_set_multicast_list(struct net_device *dev);
#endif

void hdd_wlan_initial_scan(hdd_adapter_t *pAdapter);
int isWDresetInProgress(void);

extern int hdd_setBand_helper(struct net_device *dev, tANI_U8* ptr);
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX) || defined(FEATURE_WLAN_LFR)
void hdd_getBand_helper(hdd_context_t *pHddCtx, int *pBand);
static VOS_STATUS hdd_parse_channellist(tANI_U8 *pValue, tANI_U8 *pChannelList, tANI_U8 *pNumChannels);
static VOS_STATUS hdd_parse_send_action_frame_data(tANI_U8 *pValue, tANI_U8 *pTargetApBssid,
                              tANI_U8 *pChannel, tANI_U8 *pDwellTime,
                              tANI_U8 **pBuf, tANI_U8 *pBufLen);
static VOS_STATUS hdd_parse_reassoc_command_data(tANI_U8 *pValue,
                                                 tANI_U8 *pTargetApBssid,
                                                 tANI_U8 *pChannel);
#endif
static int hdd_netdev_notifier_call(struct notifier_block * nb,
                                         unsigned long state,
                                         void *ndev)
{
   struct net_device *dev = ndev;
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_context_t *pHddCtx;
#ifdef WLAN_BTAMP_FEATURE
   VOS_STATUS status;
   hdd_context_t *pHddCtx;
#endif

   //Make sure that this callback corresponds to our device.
   if ((strncmp(dev->name, "wlan", 4)) &&
      (strncmp(dev->name, "p2p", 3)))
      return NOTIFY_DONE;

   if (isWDresetInProgress())
      return NOTIFY_DONE;

   if (!dev->ieee80211_ptr)
      return NOTIFY_DONE;

   if (NULL == pAdapter)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD Adapter Null Pointer", __func__);
      VOS_ASSERT(0);
      return NOTIFY_DONE;
   }

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   if (NULL == pHddCtx)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD Context Null Pointer", __func__);
      VOS_ASSERT(0);
      return NOTIFY_DONE;
   }

   hddLog(VOS_TRACE_LEVEL_INFO, "%s: %s New Net Device State = %lu",
          __func__, dev->name, state);

   switch (state) {
   case NETDEV_REGISTER:
        break;

   case NETDEV_UNREGISTER:
        break;

   case NETDEV_UP:
        break;

   case NETDEV_DOWN:
        break;

   case NETDEV_CHANGE:
        if(TRUE == pAdapter->isLinkUpSvcNeeded)
           complete(&pAdapter->linkup_event_var);
        break;

   case NETDEV_GOING_DOWN:
        if( pHddCtx->scan_info.mScanPending != FALSE )
        { 
           int result;
           INIT_COMPLETION(pHddCtx->scan_info.abortscan_event_var);
           hdd_abort_mac_scan(pAdapter->pHddCtx);
           result = wait_for_completion_interruptible_timeout(
                               &pHddCtx->scan_info.abortscan_event_var,
                               msecs_to_jiffies(WLAN_WAIT_TIME_ABORTSCAN));
           if(!result)
           {
              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                         "%s: Timeout occurred while waiting for abortscan" ,
                          __func__);
           }
        }
        else
        {
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
               "%s: Scan is not Pending from user" , __func__);
        }
#ifdef WLAN_BTAMP_FEATURE
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,"%s: disabling AMP", __func__);
        status = WLANBAP_StopAmp();
        if(VOS_STATUS_SUCCESS != status )
        {
           pHddCtx->isAmpAllowed = VOS_TRUE;
           hddLog(VOS_TRACE_LEVEL_FATAL,
                  "%s: Failed to stop AMP", __func__);
        }
        else
        {
           //a state m/c implementation in PAL is TBD to avoid this delay
           msleep(500);
           if ( pHddCtx->isAmpAllowed )
           {
                WLANBAP_DeregisterFromHCI();
                pHddCtx->isAmpAllowed = VOS_FALSE;
           }
        }
#endif //WLAN_BTAMP_FEATURE
        break;

   default:
        break;
   }

   return NOTIFY_DONE;
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
 * con_mode is changed by userspace to indicate a mode change which will
 * result in calling the module exit and init functions. The module
 * exit function will clean up based on the value of con_mode prior to it
 * being changed by userspace. So curr_con_mode records the current con_mode 
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
   wpt_tracelevel level;

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


/**---------------------------------------------------------------------------

  \brief hdd_wdi_trace_enable() - Configure initial WDI Trace enable

  Called immediately after the cfg.ini is read in order to configure
  the desired trace levels in the WDI.

  \param  - moduleId - module whose trace level is being configured
  \param  - bitmask - bitmask of log levels to be enabled

  \return - void

  --------------------------------------------------------------------------*/
static void hdd_wdi_trace_enable(wpt_moduleid moduleId, v_U32_t bitmask)
{
   wpt_tracelevel level;

   /* if the bitmask is the default value, then a bitmask was not
      specified in cfg.ini, so leave the logging level alone (it
      will remain at the "compiled in" default value) */
   if (CFG_WDI_TRACE_ENABLE_DEFAULT == bitmask)
   {
      return;
   }

   /* a mask was specified.  start by disabling all logging */
   wpalTraceSetLevel(moduleId, eWLAN_PAL_TRACE_LEVEL_NONE, 0);

   /* now cycle through the bitmask until all "set" bits are serviced */
   level = eWLAN_PAL_TRACE_LEVEL_FATAL;
   while (0 != bitmask)
   {
      if (bitmask & 1)
      {
         wpalTraceSetLevel(moduleId, level, 1);
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
    ENTER();

    if (NULL == pHddCtx || NULL == pHddCtx->cfg_ini)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: HDD context is Null", __func__);
        return -ENODEV;
    }

    if (pHddCtx->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: LOGP in Progress. Ignore!!!", __func__);
        return -EAGAIN;
    }

    if (pHddCtx->isLoadUnloadInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Unloading/Loading in Progress. Ignore!!!", __func__);
        return -EAGAIN;
    }
    return 0;
}

void hdd_checkandupdate_phymode( hdd_adapter_t *pAdapter, char *country_code)
{
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    hdd_config_t *cfg_param;
    eCsrPhyMode phyMode;

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

    phyMode = sme_GetPhyMode(WLAN_HDD_GET_HAL_CTX(pAdapter));

    if (NULL != strstr(cfg_param->listOfNon11acCountryCode, country_code))
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
            wait_for_completion_interruptible_timeout(&pAdapter->disconnect_comp_var,
                  msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));

    }
}

void hdd_checkandupdate_dfssetting( hdd_adapter_t *pAdapter, char *country_code)
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

int hdd_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_priv_data_t priv_data;
   tANI_U8 *command = NULL;
   int ret = 0;

   if (NULL == pAdapter)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
         "%s: HDD adapter context is Null", __func__);
      ret = -ENODEV;
      goto exit; 
   }

   if ((!ifr) || (!ifr->ifr_data))
   {
       ret = -EINVAL;
       goto exit; 
   }

   if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
       ret = -EBUSY;
       goto exit;
   }

   if (copy_from_user(&priv_data, ifr->ifr_data, sizeof(hdd_priv_data_t)))
   {
       ret = -EFAULT;
       goto exit;
   }

   command = kmalloc(priv_data.total_len, GFP_KERNEL);
   if (!command)
   {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
          "%s: failed to allocate memory\n", __func__);
       ret = -ENOMEM;
       goto exit;
   }

   if (copy_from_user(command, priv_data.buf, priv_data.total_len))
   {
       ret = -EFAULT;
       goto exit;
   }

   if ((SIOCDEVPRIVATE + 1) == cmd)
   {
       hdd_context_t *pHddCtx = (hdd_context_t*)pAdapter->pHddCtx;

       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                  "%s: Received %s cmd from Wi-Fi GUI***", __func__, command);

       if (strncmp(command, "P2P_DEV_ADDR", 12) == 0 )
       {
           if (copy_to_user(priv_data.buf, pHddCtx->p2pDeviceAddress.bytes,
                                                           sizeof(tSirMacAddr)))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s: failed to copy data to user buffer\n", __func__);
               ret = -EFAULT;
           }
       }
       else if(strncmp(command, "SETBAND", 7) == 0)
       {
           tANI_U8 *ptr = command ;
           int ret = 0 ;

           /* Change band request received */

           /* First 8 bytes will have "SETBAND " and
            * 9 byte will have band setting value */
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                    "%s: SetBandCommand Info  comm %s UL %d, TL %d", __func__, command, priv_data.used_len, priv_data.total_len);
           /* Change band request received */
           ret = hdd_setBand_helper(dev, ptr);
       }
       else if ( strncasecmp(command, "COUNTRY", 7) == 0 )
       {
           char *country_code;

           country_code = command + 8;

           hdd_checkandupdate_dfssetting(pAdapter, country_code);
           hdd_checkandupdate_phymode(pAdapter, country_code);
           ret = (int)sme_ChangeCountryCode(pHddCtx->hHal, NULL, country_code,
                    pAdapter, pHddCtx->pvosContext, eSIR_TRUE);
           if( 0 != ret )
           {
               VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                       "%s: SME Change Country code fail ret=%d\n",__func__, ret);

           }
       }
       /*
          command should be a string having format
          SET_SAP_CHANNEL_LIST <num of channels> <the channels seperated by spaces>
       */
       else if(strncmp(command, "SET_SAP_CHANNEL_LIST", 20) == 0)
       {
           tANI_U8 *ptr = command;

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      " Received Command to Set Preferred Channels for SAP in %s", __func__);

           ret = sapSetPreferredChannel(ptr);
       }
       else if(strncmp(command, "SETSUSPENDMODE", 14) == 0)
       {
           int suspend = 0;
           tANI_U8 *ptr = (tANI_U8*)command + 15;

           suspend = *ptr - '0';
           hdd_set_wlan_suspend_mode(suspend);
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

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set Roam trigger"
                      " (Neighbor lookup threshold) = %d", __func__, lookUpThreshold);

           pHddCtx->cfg_ini->nNeighborLookupRssiThreshold = lookUpThreshold;
           status = sme_setNeighborLookupRssiThreshold((tHalHandle)(pHddCtx->hHal), lookUpThreshold);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to set roam trigger, try again", __func__);
               ret = -EPERM;
               goto exit;
           }

           /* Set Reassoc threshold to (lookup rssi threshold + 5 dBm) */
           sme_setNeighborReassocRssiThreshold((tHalHandle)(pHddCtx->hHal), lookUpThreshold + 5);
       }
       else if (strncmp(command, "GETROAMTRIGGER", 14) == 0)
       {
           tANI_U8 lookUpThreshold = sme_getNeighborLookupRssiThreshold((tHalHandle)(pHddCtx->hHal));
           int rssi = (-1) * lookUpThreshold;
           char extra[32];
           tANI_U8 len = 0;

           len = snprintf(extra, sizeof(extra), "%s %d", command, rssi);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
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
           neighborEmptyScanRefreshPeriod = roamScanPeriod * 1000;

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set roam scan period"
                      " (Empty Scan refresh period) = %d", __func__, roamScanPeriod);

           pHddCtx->cfg_ini->nEmptyScanRefreshPeriod = neighborEmptyScanRefreshPeriod;
           sme_UpdateEmptyScanRefreshPeriod((tHalHandle)(pHddCtx->hHal), neighborEmptyScanRefreshPeriod);
       }
       else if (strncmp(command, "GETROAMSCANPERIOD", 17) == 0)
       {
           tANI_U16 nEmptyScanRefreshPeriod = sme_getEmptyScanRefreshPeriod((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           len = snprintf(extra, sizeof(extra), "%s %d", "GETROAMSCANPERIOD", (nEmptyScanRefreshPeriod/1000));
           /* Returned value is in units of seconds */
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
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
           sme_setNeighborScanRefreshPeriod((tHalHandle)(pHddCtx->hHal), neighborScanRefreshPeriod);
       }
       else if (strncmp(command, "GETROAMSCANREFRESHPERIOD", 24) == 0)
       {
           tANI_U16 value = sme_getNeighborScanRefreshPeriod((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           len = snprintf(extra, sizeof(extra), "%s %d", "GETROAMSCANREFRESHPERIOD", (value/1000));
           /* Returned value is in units of seconds */
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
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

	   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
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
           sme_UpdateIsFastRoamIniFeatureEnabled((tHalHandle)(pHddCtx->hHal), roamMode);
       }
       /* GETROAMMODE */
       else if (strncmp(priv_data.buf, "GETROAMMODE", SIZE_OF_GETROAMMODE) == 0)
       {
	   tANI_BOOLEAN roamMode = sme_getIsLfrFeatureEnabled((tHalHandle)(pHddCtx->hHal));
	   char extra[32];
	   tANI_U8 len = 0;

           /*
            * roamMode value shall be inverted because the sementics is different.
            */
           if (CFG_LFR_FEATURE_ENABLED_MIN == roamMode)
	       roamMode = CFG_LFR_FEATURE_ENABLED_MAX;
           else
	       roamMode = CFG_LFR_FEATURE_ENABLED_MIN;

	   len = snprintf(extra, sizeof(extra), "%s %d", command, roamMode);
	   if (copy_to_user(priv_data.buf, &extra, len + 1))
	   {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
	   }
       }
#endif
#endif
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX) || defined(FEATURE_WLAN_LFR)
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
           sme_UpdateRoamRssiDiff((tHalHandle)(pHddCtx->hHal), roamRssiDiff);
       }
       else if (strncmp(priv_data.buf, "GETROAMDELTA", 12) == 0)
       {
           tANI_U8 roamRssiDiff = sme_getRoamRssiDiff((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           len = snprintf(extra, sizeof(extra), "%s %d", command, roamRssiDiff);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
#endif
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX) || defined(FEATURE_WLAN_LFR)
       else if (strncmp(command, "GETBAND", 7) == 0)
       {
           int band = -1;
           char extra[32];
           tANI_U8 len = 0;
           hdd_getBand_helper(pHddCtx, &band);

           len = snprintf(extra, sizeof(extra), "%s %d", command, band);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "SETROAMSCANCHANNELS", 19) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 ChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
           tANI_U8 numChannels = 0;
           eHalStatus status = eHAL_STATUS_SUCCESS;

           status = hdd_parse_channellist(value, ChannelList, &numChannels);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to parse channel list information", __func__);
               ret = -EINVAL;
               goto exit;
           }

           if (numChannels > WNI_CFG_VALID_CHANNEL_LIST_LEN)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: number of channels (%d) supported exceeded max (%d)", __func__,
                   numChannels, WNI_CFG_VALID_CHANNEL_LIST_LEN);
               ret = -EINVAL;
               goto exit;
           }
           status = sme_ChangeRoamScanChannelList((tHalHandle)(pHddCtx->hHal), ChannelList,
                                                  numChannels);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to update channel list information", __func__);
               ret = -EINVAL;
               goto exit;
           }
       }
       else if (strncmp(command, "GETROAMSCANCHANNELS", 19) == 0)
       {
           tANI_U8 ChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
           tANI_U8 numChannels = 0;
           tANI_U8 j = 0;
           char extra[128] = {0};
           int len;

           if (eHAL_STATUS_SUCCESS != sme_getRoamScanChannelList( (tHalHandle)(pHddCtx->hHal),
                                              ChannelList, &numChannels ))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s: failed to get roam scan channel list", __func__);
               ret = -EFAULT;
               goto exit;
           }
           /* output channel list is of the format
           [Number of roam scan channels][Channel1][Channel2]... */
           /* copy the number of channels in the 0th index */
           len = snprintf(extra, sizeof(extra), "%s %d", command, numChannels);
           for (j = 0; (j < numChannels); j++)
           {
               len += snprintf(extra + len, sizeof(extra) - len, " %d", ChannelList[j]);
           }

           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "GETCCXMODE", 10) == 0)
       {
           tANI_BOOLEAN ccxMode = sme_getIsCcxFeatureEnabled((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           /* Check if the features OKC/CCX/11R are supported simultaneously,
              then this operation is not permitted (return FAILURE) */
           if (ccxMode &&
               hdd_is_okc_mode_enabled(pHddCtx) &&
               sme_getIsFtFeatureEnabled((tHalHandle)(pHddCtx->hHal)))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                  "%s: OKC/CCX/11R are supported simultaneously"
                  " hence this operation is not permitted!", __func__);
               ret = -EPERM;
               goto exit;
           }

           len = snprintf(extra, sizeof(extra), "%s %d", "GETCCXMODE", ccxMode);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
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

           /* Check if the features OKC/CCX/11R are supported simultaneously,
              then this operation is not permitted (return FAILURE) */
           if (okcMode &&
               sme_getIsCcxFeatureEnabled((tHalHandle)(pHddCtx->hHal)) &&
               sme_getIsFtFeatureEnabled((tHalHandle)(pHddCtx->hHal)))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                  "%s: OKC/CCX/11R are supported simultaneously"
                  " hence this operation is not permitted!", __func__);
               ret = -EPERM;
               goto exit;
           }

           len = snprintf(extra, sizeof(extra), "%s %d", "GETOKCMODE", okcMode);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "GETFASTROAM", 11) == 0)
       {
           tANI_BOOLEAN lfrMode = sme_getIsLfrFeatureEnabled((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           len = snprintf(extra, sizeof(extra), "%s %d", "GETFASTROAM", lfrMode);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "GETFASTTRANSITION", 17) == 0)
       {
           tANI_BOOLEAN ft = sme_getIsFtFeatureEnabled((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           len = snprintf(extra, sizeof(extra), "%s %d", "GETFASTTRANSITION", ft);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
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

           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change channel min time = %d", __func__, minTime);

           pHddCtx->cfg_ini->nNeighborScanMinChanTime = minTime;
           sme_setNeighborScanMinChanTime((tHalHandle)(pHddCtx->hHal), minTime);
       }
       else if (strncmp(command, "SENDACTIONFRAME", 15) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 channel = 0;
           tANI_U8 dwellTime = 0;
           tANI_U8 bufLen = 0;
           tANI_U8 *buf = NULL;
           tSirMacAddr targetApBssid;
           eHalStatus status = eHAL_STATUS_SUCCESS;
           struct ieee80211_channel chan;
           tANI_U8 finalLen = 0;
           tANI_U8 *finalBuf = NULL;
           tANI_U8 temp = 0;
           u64 cookie;
           hdd_station_ctx_t *pHddStaCtx = NULL;
           pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

           /* if not associated, no need to send action frame */
           if (eConnectionState_Associated != pHddStaCtx->conn_info.connState)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s:Not associated!",__func__);
               ret = -EINVAL;
               goto exit;
           }

           status = hdd_parse_send_action_frame_data(value, targetApBssid, &channel,
                                                     &dwellTime, &buf, &bufLen);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to parse send action frame data", __func__);
               ret = -EINVAL;
               goto exit;
           }

           /* if the target bssid is different from currently associated AP,
              then no need to send action frame */
           if (VOS_TRUE != vos_mem_compare(targetApBssid,
                                           pHddStaCtx->conn_info.bssId, sizeof(tSirMacAddr)))
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s:STA is not associated to this AP!",__func__);
               ret = -EINVAL;
               vos_mem_free(buf);
               goto exit;
           }

           /* if the channel number is different from operating channel then
              no need to send action frame */
           if (channel != pHddStaCtx->conn_info.operationChannel)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                         "%s: channel(%d) is different from operating channel(%d)",
                         __func__, channel, pHddStaCtx->conn_info.operationChannel);
               ret = -EINVAL;
               vos_mem_free(buf);
               goto exit;
           }
           chan.center_freq = sme_ChnToFreq(channel);

           finalLen = bufLen + 24;
           finalBuf = vos_mem_malloc(finalLen);
           if (NULL == finalBuf)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, "%s:memory allocation failed",__func__);
               ret = -ENOMEM;
               vos_mem_free(buf);
               goto exit;
           }
           vos_mem_zero(finalBuf, finalLen);

           /* Fill subtype */
           temp = SIR_MAC_MGMT_ACTION << 4;
           vos_mem_copy(finalBuf + 0, &temp, sizeof(temp));

           /* Fill type */
           temp = SIR_MAC_MGMT_FRAME;
           vos_mem_copy(finalBuf + 2, &temp, sizeof(temp));

           /* Fill destination address (bssid of the AP) */
           vos_mem_copy(finalBuf + 4, targetApBssid, sizeof(targetApBssid));

           /* Fill source address (STA mac address) */
           vos_mem_copy(finalBuf + 10, pAdapter->macAddressCurrent.bytes, sizeof(pAdapter->macAddressCurrent.bytes));

           /* Fill BSSID (AP mac address) */
           vos_mem_copy(finalBuf + 16, targetApBssid, sizeof(targetApBssid));

           /* Fill received buffer from 24th address */
           vos_mem_copy(finalBuf + 24, buf, bufLen);

           /* done with the parsed buffer */
           vos_mem_free(buf);

           wlan_hdd_action( NULL,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
                       &(pAdapter->wdev),
#else
                       dev,
#endif
                       &chan, 0,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
                       NL80211_CHAN_HT20, 1,
#endif
                       dwellTime, finalBuf, finalLen,  1,
                       1, &cookie );
           vos_mem_free(finalBuf);
       }
       else if (strncmp(command, "GETROAMSCANCHANNELMINTIME", 25) == 0)
       {
           tANI_U16 val = sme_getNeighborScanMinChanTime((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           /* value is interms of msec */
           len = snprintf(extra, sizeof(extra), "%s %d", "GETROAMSCANCHANNELMINTIME", val);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
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
           tANI_U16 homeAwayTime = 0;

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

           /* Home Away Time should be atleast equal to (MaxDwell time + (2*RFS)),
           *  where RFS is the RF Switching time. It is twice RFS to consider the
           *  time to go off channel and return to the home channel. */
           homeAwayTime = sme_getRoamScanHomeAwayTime((tHalHandle)(pHddCtx->hHal));
           if (homeAwayTime < (maxTime + (2 * HDD_ROAM_SCAN_CHANNEL_SWITCH_TIME)))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                      "%s: Invalid config, Home away time(%d) is less than (twice RF switching time + channel max time)(%d)",
                      " Hence enforcing home away time to disable (0)",
                      __func__, homeAwayTime, (maxTime + (2 * HDD_ROAM_SCAN_CHANNEL_SWITCH_TIME)));
               homeAwayTime = 0;
               pHddCtx->cfg_ini->nRoamScanHomeAwayTime = homeAwayTime;
               sme_UpdateRoamScanHomeAwayTime((tHalHandle)(pHddCtx->hHal), homeAwayTime, eANI_BOOLEAN_FALSE);
           }
           sme_setNeighborScanMaxChanTime((tHalHandle)(pHddCtx->hHal), maxTime);
       }
       else if (strncmp(command, "GETSCANCHANNELTIME", 18) == 0)
       {
           tANI_U16 val = sme_getNeighborScanMaxChanTime((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           /* value is interms of msec */
           len = snprintf(extra, sizeof(extra), "%s %d", "GETSCANCHANNELTIME", val);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
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
           sme_setNeighborScanPeriod((tHalHandle)(pHddCtx->hHal), val);
       }
       else if (strncmp(command, "GETSCANHOMETIME", 15) == 0)
       {
           tANI_U16 val = sme_getNeighborScanPeriod((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           /* value is interms of msec */
           len = snprintf(extra, sizeof(extra), "%s %d", "GETSCANHOMETIME", val);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
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
           sme_setRoamIntraBand((tHalHandle)(pHddCtx->hHal), val);
       }
       else if (strncmp(command, "GETROAMINTRABAND", 16) == 0)
       {
           tANI_U16 val = sme_getRoamIntraBand((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           /* value is interms of msec */
           len = snprintf(extra, sizeof(extra), "%s %d", "GETROAMINTRABAND", val);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
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
           sme_UpdateRoamScanNProbes((tHalHandle)(pHddCtx->hHal), nProbes);
       }
       else if (strncmp(priv_data.buf, "GETSCANNPROBES", 14) == 0)
       {
           tANI_U8 val = sme_getRoamScanNProbes((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           len = snprintf(extra, sizeof(extra), "%s %d", command, val);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
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
           tANI_U16 scanChannelMaxTime = 0;

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

           /* Home Away Time should be atleast equal to (MaxDwell time + (2*RFS)),
           *  where RFS is the RF Switching time. It is twice RFS to consider the
           *  time to go off channel and return to the home channel. */
           scanChannelMaxTime = sme_getNeighborScanMaxChanTime((tHalHandle)(pHddCtx->hHal));
           if (homeAwayTime < (scanChannelMaxTime + (2 * HDD_ROAM_SCAN_CHANNEL_SWITCH_TIME)))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                      "%s: Invalid config, Home away time(%d) is less than (twice RF switching time + channel max time)(%d)",
                      " Hence enforcing home away time to disable (0)",
                      __func__, homeAwayTime, (scanChannelMaxTime + (2 * HDD_ROAM_SCAN_CHANNEL_SWITCH_TIME)));
               homeAwayTime = 0;
           }

           if (pHddCtx->cfg_ini->nRoamScanHomeAwayTime != homeAwayTime)
           {
               pHddCtx->cfg_ini->nRoamScanHomeAwayTime = homeAwayTime;
               sme_UpdateRoamScanHomeAwayTime((tHalHandle)(pHddCtx->hHal), homeAwayTime, eANI_BOOLEAN_TRUE);
           }
       }
       else if (strncmp(priv_data.buf, "GETSCANHOMEAWAYTIME", 19) == 0)
       {
           tANI_U16 val = sme_getRoamScanHomeAwayTime((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           len = snprintf(extra, sizeof(extra), "%s %d", command, val);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
       else if (strncmp(command, "REASSOC", 7) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 channel = 0;
           tSirMacAddr targetApBssid;
           eHalStatus status = eHAL_STATUS_SUCCESS;
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
           tCsrHandoffRequest handoffInfo;
#endif
           hdd_station_ctx_t *pHddStaCtx = NULL;
           pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

           /* if not associated, no need to proceed with reassoc */
           if (eConnectionState_Associated != pHddStaCtx->conn_info.connState)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s:Not associated!",__func__);
               ret = -EINVAL;
               goto exit;
           }

           status = hdd_parse_reassoc_command_data(value, targetApBssid, &channel);
           if (eHAL_STATUS_SUCCESS != status)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to parse reassoc command data", __func__);
               ret = -EINVAL;
               goto exit;
           }

           /* if the target bssid is same as currently associated AP,
              then no need to proceed with reassoc */
           if (VOS_TRUE == vos_mem_compare(targetApBssid,
                                           pHddStaCtx->conn_info.bssId, sizeof(tSirMacAddr)))
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s:Reassoc BSSID is same as currently associated AP bssid",__func__);
               ret = -EINVAL;
               goto exit;
           }

           /* Check channel number is a valid channel number */
           if(VOS_STATUS_SUCCESS !=
                         wlan_hdd_validate_operation_channel(pAdapter, channel))
           {
               hddLog(VOS_TRACE_LEVEL_ERROR,
                      "%s: Invalid Channel [%d] \n", __func__, channel);
               return -EINVAL;
           }

           /* Proceed with reassoc */
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
           handoffInfo.channel = channel;
           vos_mem_copy(handoffInfo.bssid, targetApBssid, sizeof(tSirMacAddr));
           sme_HandoffRequest(pHddCtx->hHal, &handoffInfo);
#endif
       }
#endif
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
           sme_UpdateIsFastRoamIniFeatureEnabled((tHalHandle)(pHddCtx->hHal), lfrMode);
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
           sme_UpdateFastTransitionEnabled((tHalHandle)(pHddCtx->hHal), ft);
       }
#endif
#ifdef FEATURE_WLAN_CCX
       else if (strncmp(command, "SETCCXMODE", 10) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 ccxMode = CFG_CCX_FEATURE_ENABLED_DEFAULT;

           /* Check if the features OKC/CCX/11R are supported simultaneously,
              then this operation is not permitted (return FAILURE) */
           if (sme_getIsCcxFeatureEnabled((tHalHandle)(pHddCtx->hHal)) &&
               hdd_is_okc_mode_enabled(pHddCtx) &&
               sme_getIsFtFeatureEnabled((tHalHandle)(pHddCtx->hHal)))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                  "%s: OKC/CCX/11R are supported simultaneously"
                  " hence this operation is not permitted!", __func__);
               ret = -EPERM;
               goto exit;
           }

           /* Move pointer to ahead of SETCCXMODE<delimiter> */
           value = value + 11;
           /* Convert the value from ascii to integer */
           ret = kstrtou8(value, 10, &ccxMode);
           if (ret < 0)
           {
               /* If the input value is greater than max value of datatype, then also
                  kstrtou8 fails */
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: kstrtou8 failed range [%d - %d]", __func__,
                      CFG_CCX_FEATURE_ENABLED_MIN,
                      CFG_CCX_FEATURE_ENABLED_MAX);
               ret = -EINVAL;
               goto exit;
           }
           if ((ccxMode < CFG_CCX_FEATURE_ENABLED_MIN) ||
               (ccxMode > CFG_CCX_FEATURE_ENABLED_MAX))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "Ccx mode value %d is out of range"
                      " (Min: %d Max: %d)", ccxMode,
                      CFG_CCX_FEATURE_ENABLED_MIN,
                      CFG_CCX_FEATURE_ENABLED_MAX);
               ret = -EINVAL;
               goto exit;
           }
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to change ccx mode = %d", __func__, ccxMode);

           pHddCtx->cfg_ini->isCcxIniFeatureEnabled = ccxMode;
           sme_UpdateIsCcxFeatureEnabled((tHalHandle)(pHddCtx->hHal), ccxMode);
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

           if (0 != roamScanControl)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "roam scan control invalid value = %d",
                      roamScanControl);
               ret = -EINVAL;
               goto exit;
           }
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: Received Command to Set roam scan control = %d", __func__, roamScanControl);

           sme_SetRoamScanControl((tHalHandle)(pHddCtx->hHal), roamScanControl);
       }
#ifdef FEATURE_WLAN_OKC
       else if (strncmp(command, "SETOKCMODE", 10) == 0)
       {
           tANI_U8 *value = command;
           tANI_U8 okcMode = CFG_OKC_FEATURE_ENABLED_DEFAULT;

           /* Check if the features OKC/CCX/11R are supported simultaneously,
              then this operation is not permitted (return FAILURE) */
           if (sme_getIsCcxFeatureEnabled((tHalHandle)(pHddCtx->hHal)) &&
               hdd_is_okc_mode_enabled(pHddCtx) &&
               sme_getIsFtFeatureEnabled((tHalHandle)(pHddCtx->hHal)))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                  "%s: OKC/CCX/11R are supported simultaneously"
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
       else if (strncmp(priv_data.buf, "GETROAMSCANCONTROL", 18) == 0)
       {
           tANI_BOOLEAN roamScanControl = sme_GetRoamScanControl((tHalHandle)(pHddCtx->hHal));
           char extra[32];
           tANI_U8 len = 0;

           len = snprintf(extra, sizeof(extra), "%s %d", command, roamScanControl);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
               ret = -EFAULT;
               goto exit;
           }
       }
#endif
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
           char *dhcpPhase;
           dhcpPhase = command + 12;
           if ('1' == *dhcpPhase)
           {
               sme_DHCPStartInd(pHddCtx->hHal, pAdapter->device_mode,
                                pAdapter->macAddressCurrent.bytes);
           }
           else if ('2' == *dhcpPhase)
           {
               sme_DHCPStopInd(pHddCtx->hHal, pAdapter->device_mode,
                               pAdapter->macAddressCurrent.bytes);
           }
       }
       else if (strncmp(command, "SCAN-ACTIVE", 11) == 0)
       {
          pHddCtx->scan_info.scan_mode = eSIR_ACTIVE_SCAN;
       }
       else if (strncmp(command, "SCAN-PASSIVE", 12) == 0)
       {
          pHddCtx->scan_info.scan_mode = eSIR_PASSIVE_SCAN;
       }
       else if (strncmp(command, "GETDWELLTIME", 12) == 0)
       {
           hdd_config_t *pCfg = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini;
           char extra[32];
           tANI_U8 len = 0;

           len = snprintf(extra, sizeof(extra), "GETDWELLTIME %u\n",
                  (int)pCfg->nActiveMaxChnTime);
           if (copy_to_user(priv_data.buf, &extra, len + 1))
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
           tANI_U8 *value = command;
           hdd_config_t *pCfg = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini;
           int val = 0, temp;

           value = value + 13;
           temp = kstrtou32(value, 10, &val);
           if ( temp != 0 || val < CFG_ACTIVE_MAX_CHANNEL_TIME_MIN ||
                             val > CFG_ACTIVE_MAX_CHANNEL_TIME_MAX )
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: argument passed for SETDWELLTIME is incorrect", __func__);
               ret = -EFAULT;
               goto exit;
           }
           pCfg->nActiveMaxChnTime = val;
       }
       else if ( strncasecmp(command, "MIRACAST", 8) == 0 )
       {
           tANI_U8 filterType = 0;
           tANI_U8 *value;
           value = command + 9;

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
           if ((filterType < WLAN_HDD_DRIVER_MIRACAST_CFG_MIN_VAL ) ||
               (filterType > WLAN_HDD_DRIVER_MIRACAST_CFG_MAX_VAL))
           {
               VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: Accepted Values are 0 to 2. 0-Disabled, 1-Source,"
                      " 2-Sink ", __func__);
               ret = -EINVAL;
               goto exit;
           }
           //Filtertype value should be either 0-Disabled, 1-Source, 2-sink
           pHddCtx->drvr_miracast = filterType;
           hdd_tx_rx_pkt_cnt_stat_timer_handler(pHddCtx);
        }
       else if (strncmp(command, "SETMCRATE", 9) == 0)
       {
           int      rc;
           tANI_U8 *value = command;
           int      targetRate;

           /* Only valid for SAP mode */
           if (WLAN_HDD_SOFTAP != pAdapter->device_mode)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: SAP mode is not running", __func__);
               ret = -EFAULT;
               goto exit;
           }

           /* Move pointer to ahead of SETMCRATE<delimiter> */
           /* input value is in units of hundred kbps */
           value = value + 10;
           /* Convert the value from ascii to integer, decimal base */
           ret = kstrtouint(value, 10, &targetRate);

           rc = hdd_hostapd_set_mc_rate(pAdapter, targetRate);
           if (rc)
           {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Set MC Rate Fail %d", __func__, rc);
               ret = -EFAULT;
               goto exit;
           }
       }
       else {
           hddLog( VOS_TRACE_LEVEL_WARN, "%s: Unsupported GUI command %s",
                   __func__, command);
       }

   }
exit:
   if (command)
   {
       kfree(command);
   }
   return ret;
}

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX) || defined(FEATURE_WLAN_LFR)
void hdd_getBand_helper(hdd_context_t *pHddCtx, int *pBand)
{
    eCsrBand band = -1;
    sme_GetFreqBand((tHalHandle)(pHddCtx->hHal), &band);
    switch (band)
    {
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
            hddLog( VOS_TRACE_LEVEL_WARN, "%s: Invalid Band %d", __func__, band);
            *pBand = -1;
            break;
    }
}

/**---------------------------------------------------------------------------

  \brief hdd_parse_send_action_frame_data() - HDD Parse send action frame data

  This function parses the send action frame data passed in the format
  SENDACTIONFRAME<space><bssid><space><channel><space><dwelltime><space><data>

  \param  - pValue Pointer to input data
  \param  - pTargetApBssid Pointer to target Ap bssid
  \param  - pChannel Pointer to the Target AP channel
  \param  - pDwellTime Pointer to the time to stay off-channel after transmitting action frame
  \param  - pBuf Pointer to data
  \param  - pBufLen Pointer to data length

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
VOS_STATUS hdd_parse_send_action_frame_data(tANI_U8 *pValue, tANI_U8 *pTargetApBssid, tANI_U8 *pChannel,
                                            tANI_U8 *pDwellTime, tANI_U8 **pBuf, tANI_U8 *pBufLen)
{
    tANI_U8 *inPtr = pValue;
    tANI_U8 *dataEnd;
    int tempInt;
    int j = 0;
    int i = 0;
    int v = 0;
    tANI_U8 tempBuf[32];
    tANI_U8 tempByte = 0;

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
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr)
    {
        return -EINVAL;
    }

    /*getting the first argument ie the target AP bssid */
    if (inPtr[2] != ':' || inPtr[5] != ':' || inPtr[8] != ':' || inPtr[11] != ':' || inPtr[14] != ':')
    {
       return -EINVAL;
    }
    j = sscanf(inPtr, "%2x:%2x:%2x:%2x:%2x:%2x", (unsigned int *)&pTargetApBssid[0], (unsigned int *)&pTargetApBssid[1],
                  (unsigned int *)&pTargetApBssid[2], (unsigned int *)&pTargetApBssid[3],
                  (unsigned int *)&pTargetApBssid[4], (unsigned int *)&pTargetApBssid[5]);

    /* point to the next argument */
    inPtr = strnchr(inPtr, strlen(inPtr), SPACE_ASCII_VALUE);
    /*no argument after the command*/
    if (NULL == inPtr) return -EINVAL;

    /*removing empty spaces*/
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr)
    {
        return -EINVAL;
    }

    /*getting the next argument ie the channel number */
    j = sscanf(inPtr, "%32s ", tempBuf);
    v = kstrtos32(tempBuf, 10, &tempInt);
    if ( v < 0) return -EINVAL;

    *pChannel = tempInt;

    /* point to the next argument */
    inPtr = strnchr(inPtr, strlen(inPtr), SPACE_ASCII_VALUE);
    /*no argument after the command*/
    if (NULL == inPtr) return -EINVAL;
    /*removing empty spaces*/
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr)
    {
        return -EINVAL;
    }

    /*getting the next argument ie the dwell time */
    j = sscanf(inPtr, "%32s ", tempBuf);
    v = kstrtos32(tempBuf, 10, &tempInt);
    if ( v < 0) return -EINVAL;

    *pDwellTime = tempInt;

    /* point to the next argument */
    inPtr = strnchr(inPtr, strlen(inPtr), SPACE_ASCII_VALUE);
    /*no argument after the command*/
    if (NULL == inPtr) return -EINVAL;
    /*removing empty spaces*/
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr)
    {
        return -EINVAL;
    }

    /* find the length of data */
    dataEnd = inPtr;
    while(('\0' !=  *dataEnd) )
    {
        dataEnd++;
        ++(*pBufLen);
    }
    if ( *pBufLen <= 0)  return -EINVAL;

    /* Allocate the number of bytes based on the number of input characters
       whether it is even or odd.
       if the number of input characters are even, then we need N/2 byte.
       if the number of input characters are odd, then we need do (N+1)/2 to
       compensate rounding off.
       For example, if N = 18, then (18 + 1)/2 = 9 bytes are enough.
       If N = 19, then we need 10 bytes, hence (19 + 1)/2 = 10 bytes */
    *pBuf = vos_mem_malloc((*pBufLen + 1)/2);
    if (NULL == *pBuf)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
           "%s: vos_mem_alloc failed ", __func__);
        return -EINVAL;
    }

    /* the buffer received from the upper layer is character buffer,
       we need to prepare the buffer taking 2 characters in to a U8 hex decimal number
       for example 7f0000f0...form a buffer to contain 7f in 0th location, 00 in 1st
       and f0 in 3rd location */
    for (i = 0, j = 0; j < *pBufLen; j += 2)
    {
        tempByte = (hdd_parse_hex(inPtr[j]) << 4) | (hdd_parse_hex(inPtr[j + 1]));
        (*pBuf)[i++] = tempByte;
    }
    *pBufLen = i;
    return VOS_STATUS_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief hdd_parse_channellist() - HDD Parse channel list

  This function parses the channel list passed in the format
  SETROAMSCANCHANNELS<space><Number of channels><space>Channel 1<space>Channel 2<space>Channel N
  if the Number of channels (N) does not match with the actual number of channels passed
  then take the minimum of N and count of (Ch1, Ch2, ...Ch M)
  For example, if SETROAMSCANCHANNELS 3 36 40 44 48, only 36, 40 and 44 shall be taken.
  If SETROAMSCANCHANNELS 5 36 40 44 48, ignore 5 and take 36, 40, 44 and 48.
  This function does not take care of removing duplicate channels from the list

  \param  - pValue Pointer to input channel list
  \param  - ChannelList Pointer to local output array to record channel list
  \param  - pNumChannels Pointer to number of roam scan channels

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
VOS_STATUS hdd_parse_channellist(tANI_U8 *pValue, tANI_U8 *pChannelList, tANI_U8 *pNumChannels)
{
    tANI_U8 *inPtr = pValue;
    int tempInt;
    int j = 0;
    int v = 0;
    char buf[32];

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
        return -EINVAL;
    }

    /*getting the first argument ie the number of channels*/
    sscanf(inPtr, "%32s ", buf);
    v = kstrtos32(buf, 10, &tempInt);
    if ((v < 0) ||
        (tempInt <= 0) ||
        (tempInt > WNI_CFG_VALID_CHANNEL_LIST_LEN))
    {
       return -EINVAL;
    }

    *pNumChannels = tempInt;

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
               "Number of channels are: %d", *pNumChannels);

    for (j = 0; j < (*pNumChannels); j++)
    {
        /*inPtr pointing to the beginning of first space after number of channels*/
        inPtr = strpbrk( inPtr, " " );
        /*no channel list after the number of channels argument*/
        if (NULL == inPtr)
        {
            if (0 != j)
            {
                *pNumChannels = j;
                return VOS_STATUS_SUCCESS;
            }
            else
            {
                return -EINVAL;
            }
        }

        /*removing empty space*/
        while ((SPACE_ASCII_VALUE == *inPtr) && ('\0' != *inPtr)) inPtr++;

        /*no channel list after the number of channels argument and spaces*/
        if ( '\0' == *inPtr )
        {
            if (0 != j)
            {
                *pNumChannels = j;
                return VOS_STATUS_SUCCESS;
            }
            else
            {
                return -EINVAL;
            }
        }

        sscanf(inPtr, "%32s ", buf);
        v = kstrtos32(buf, 10, &tempInt);
        if ((v < 0) ||
            (tempInt <= 0) ||
            (tempInt > WNI_CFG_CURRENT_CHANNEL_STAMAX))
        {
           return -EINVAL;
        }
        pChannelList[j] = tempInt;

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Channel %d added to preferred channel list",
                   pChannelList[j] );
    }

    return VOS_STATUS_SUCCESS;
}


/**---------------------------------------------------------------------------

  \brief hdd_parse_reassoc_command_data() - HDD Parse reassoc command data

  This function parses the reasoc command data passed in the format
  REASSOC<space><bssid><space><channel>

  \param  - pValue Pointer to input data
  \param  - pTargetApBssid Pointer to target Ap bssid
  \param  - pChannel Pointer to the Target AP channel

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
VOS_STATUS hdd_parse_reassoc_command_data(tANI_U8 *pValue, tANI_U8 *pTargetApBssid, tANI_U8 *pChannel)
{
    tANI_U8 *inPtr = pValue;
    int tempInt;
    int v = 0;
    tANI_U8 tempBuf[32];

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
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr)
    {
        return -EINVAL;
    }

    /*getting the first argument ie the target AP bssid */
    if (inPtr[2] != ':' || inPtr[5] != ':' || inPtr[8] != ':' || inPtr[11] != ':' || inPtr[14] != ':')
    {
       return -EINVAL;
    }
    sscanf(inPtr, "%2x:%2x:%2x:%2x:%2x:%2x", (unsigned int *)&pTargetApBssid[0], (unsigned int *)&pTargetApBssid[1],
                  (unsigned int *)&pTargetApBssid[2], (unsigned int *)&pTargetApBssid[3],
                  (unsigned int *)&pTargetApBssid[4], (unsigned int *)&pTargetApBssid[5]);

    /* point to the next argument */
    inPtr = strnchr(inPtr, strlen(inPtr), SPACE_ASCII_VALUE);
    /*no argument after the command*/
    if (NULL == inPtr) return -EINVAL;

    /*removing empty spaces*/
    while ((SPACE_ASCII_VALUE  == *inPtr) && ('\0' !=  *inPtr) ) inPtr++;

    /*no argument followed by spaces*/
    if ('\0' == *inPtr)
    {
        return -EINVAL;
    }

    /*getting the next argument ie the channel number */
    sscanf(inPtr, "%s ", tempBuf);
    v = kstrtos32(tempBuf, 10, &tempInt);
    if ( v < 0) return -EINVAL;

    *pChannel = tempInt;
    return VOS_STATUS_SUCCESS;
}

#endif

/**---------------------------------------------------------------------------

  \brief hdd_open() - HDD Open function

  This is called in response to ifconfig up

  \param  - dev Pointer to net_device structure

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
int hdd_open (struct net_device *dev)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_context_t *pHddCtx;
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   v_BOOL_t in_standby = TRUE;

   if (NULL == pAdapter) 
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
         "%s: HDD adapter context is Null", __func__);
      return -ENODEV;
   }
   
   pHddCtx = (hdd_context_t*)pAdapter->pHddCtx;
   if (NULL == pHddCtx)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
         "%s: HDD context is Null", __func__);
      return -ENODEV;
   }

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
   while ( (NULL != pAdapterNode) && (VOS_STATUS_SUCCESS == status) )
   {
      if (test_bit(DEVICE_IFACE_OPENED, &pAdapterNode->pAdapter->event_flags))
      {
         hddLog(VOS_TRACE_LEVEL_INFO, "%s: chip already out of standby",
                __func__);
         in_standby = FALSE;
         break;
      }
      else
      {
         status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
         pAdapterNode = pNext;
      }
   }
 
   if (TRUE == in_standby)
   {
       if (VOS_STATUS_SUCCESS != wlan_hdd_exit_lowpower(pHddCtx, pAdapter))
       {
           hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Failed to bring " 
                   "wlan out of power save", __func__);
           return -EINVAL;
       }
   }
   
   set_bit(DEVICE_IFACE_OPENED, &pAdapter->event_flags);
   if (hdd_connIsConnected(WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))) 
   {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                 "%s: Enabling Tx Queues", __func__);
       /* Enable TX queues only when we are connected */
       netif_tx_start_all_queues(dev);
   }

   return 0;
}

int hdd_mon_open (struct net_device *dev)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);

   if(pAdapter == NULL) {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
         "%s: HDD adapter context is Null", __func__);
      return -EINVAL;
   }

   netif_start_queue(dev);

   return 0;
}
/**---------------------------------------------------------------------------

  \brief hdd_stop() - HDD stop function

  This is called in response to ifconfig down

  \param  - dev Pointer to net_device structure

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/

int hdd_stop (struct net_device *dev)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_context_t *pHddCtx;
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   v_BOOL_t enter_standby = TRUE;
   
   ENTER();

   if (NULL == pAdapter)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
         "%s: HDD adapter context is Null", __func__);
      return -ENODEV;
   }

   pHddCtx = (hdd_context_t*)pAdapter->pHddCtx;
   if (NULL == pHddCtx)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
         "%s: HDD context is Null", __func__);
      return -ENODEV;
   }

   clear_bit(DEVICE_IFACE_OPENED, &pAdapter->event_flags);
   hddLog(VOS_TRACE_LEVEL_INFO, "%s: Disabling OS Tx queues", __func__);
   netif_tx_disable(pAdapter->dev);
   netif_carrier_off(pAdapter->dev);


   /* SoftAP ifaces should never go in power save mode
      making sure same here. */
   if ( (WLAN_HDD_SOFTAP == pAdapter->device_mode )
                 || (WLAN_HDD_MONITOR == pAdapter->device_mode )
                 || (WLAN_HDD_P2P_GO == pAdapter->device_mode )
      )
   {
      /* SoftAP mode, so return from here */
      EXIT();
      return 0;
   }

   /* Find if any iface is up then
      if any iface is up then can't put device to sleep/ power save mode. */
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
   {
       hddLog(VOS_TRACE_LEVEL_INFO, "%s: All Interfaces are Down " 
                 "entering standby", __func__);
       if (VOS_STATUS_SUCCESS != wlan_hdd_enter_lowpower(pHddCtx))
       {
           /*log and return success*/
           hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Failed to put "
                   "wlan in power save", __func__);
       }
   }
   
   EXIT();
   return 0;
}

/**---------------------------------------------------------------------------

  \brief hdd_uninit() - HDD uninit function

  This is called during the netdev unregister to uninitialize all data
associated with the device

  \param  - dev Pointer to net_device structure

  \return - void

  --------------------------------------------------------------------------*/
static void hdd_uninit (struct net_device *dev)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);

   ENTER();

   do
   {
      if (NULL == pAdapter)
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: NULL pAdapter", __func__);
         break;
      }

      if (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: Invalid magic", __func__);
         break;
      }

      if (NULL == pAdapter->pHddCtx)
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: NULL pHddCtx", __func__);
         break;
      }

      if (dev != pAdapter->dev)
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: Invalid device reference", __func__);
         /* we haven't validated all cases so let this go for now */
      }

      hdd_deinit_adapter(pAdapter->pHddCtx, pAdapter);

      /* after uninit our adapter structure will no longer be valid */
      pAdapter->dev = NULL;
      pAdapter->magic = 0;
   } while (0);

   EXIT();
}

/**---------------------------------------------------------------------------

  \brief hdd_release_firmware() -

   This function calls the release firmware API to free the firmware buffer.

  \param  - pFileName Pointer to the File Name.
                  pCtx - Pointer to the adapter .


  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

VOS_STATUS hdd_release_firmware(char *pFileName,v_VOID_t *pCtx)
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   hdd_context_t *pHddCtx = (hdd_context_t*)pCtx;
   ENTER();


   if (!strcmp(WLAN_FW_FILE, pFileName)) {
   
       hddLog(VOS_TRACE_LEVEL_INFO_HIGH,"%s: Loaded firmware file is %s",__func__,pFileName);

       if(pHddCtx->fw) {
          release_firmware(pHddCtx->fw);
          pHddCtx->fw = NULL;
       }
       else
          status = VOS_STATUS_E_FAILURE;
   }
   else if (!strcmp(WLAN_NV_FILE,pFileName)) {
       if(pHddCtx->nv) {
          release_firmware(pHddCtx->nv);
          pHddCtx->nv = NULL;
       }
       else
          status = VOS_STATUS_E_FAILURE;

   }

   EXIT();
   return status;
}

/**---------------------------------------------------------------------------

  \brief hdd_request_firmware() -

   This function reads the firmware file using the request firmware
   API and returns the the firmware data and the firmware file size.

  \param  - pfileName - Pointer to the file name.
              - pCtx - Pointer to the adapter .
              - ppfw_data - Pointer to the pointer of the firmware data.
              - pSize - Pointer to the file size.

  \return - VOS_STATUS_SUCCESS for success, VOS_STATUS_E_FAILURE for failure

  --------------------------------------------------------------------------*/


VOS_STATUS hdd_request_firmware(char *pfileName,v_VOID_t *pCtx,v_VOID_t **ppfw_data, v_SIZE_t *pSize)
{
   int status;
   VOS_STATUS retval = VOS_STATUS_SUCCESS;
   hdd_context_t *pHddCtx = (hdd_context_t*)pCtx;
   ENTER();

   if( (!strcmp(WLAN_FW_FILE, pfileName)) ) {

       status = request_firmware(&pHddCtx->fw, pfileName, pHddCtx->parent_dev);

       if(status || !pHddCtx->fw || !pHddCtx->fw->data) {
           hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Firmware %s download failed",
                  __func__, pfileName);
           retval = VOS_STATUS_E_FAILURE;
       }

       else {
         *ppfw_data = (v_VOID_t *)pHddCtx->fw->data;
         *pSize = pHddCtx->fw->size;
          hddLog(VOS_TRACE_LEVEL_INFO, "%s: Firmware size = %d",
                 __func__, *pSize);
       }
   }
   else if(!strcmp(WLAN_NV_FILE, pfileName)) {

       status = request_firmware(&pHddCtx->nv, pfileName, pHddCtx->parent_dev);

       if(status || !pHddCtx->nv || !pHddCtx->nv->data) {
           hddLog(VOS_TRACE_LEVEL_FATAL, "%s: nv %s download failed",
                  __func__, pfileName);
           retval = VOS_STATUS_E_FAILURE;
       }

       else {
         *ppfw_data = (v_VOID_t *)pHddCtx->nv->data;
         *pSize = pHddCtx->nv->size;
          hddLog(VOS_TRACE_LEVEL_INFO, "%s: nv file size = %d",
                 __func__, *pSize);
       }
   }

   EXIT();
   return retval;
}
/**---------------------------------------------------------------------------
     \brief hdd_full_pwr_cbk() - HDD full power callbackfunction

      This is the function invoked by SME to inform the result of a full power
      request issued by HDD

     \param  - callbackcontext - Pointer to cookie
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

/**---------------------------------------------------------------------------

    \brief hdd_req_bmps_cbk() - HDD Request BMPS callback function

     This is the function invoked by SME to inform the result of BMPS
     request issued by HDD

    \param  - callbackcontext - Pointer to cookie
               status - result of request

    \return - None

--------------------------------------------------------------------------*/
void hdd_req_bmps_cbk(void *callbackContext, eHalStatus status)
{

   struct completion *completion_var = (struct completion*) callbackContext;

   hddLog(VOS_TRACE_LEVEL_ERROR, "HDD BMPS request Callback, status = %d\n", status);
   if(completion_var != NULL)
   {
      complete(completion_var);
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

/**---------------------------------------------------------------------------

  \brief hdd_set_mac_address() -

   This function sets the user specified mac address using
   the command ifconfig wlanX hw ether <mac adress>.

  \param  - dev - Pointer to the net device.
              - addr - Pointer to the sockaddr.
  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static int hdd_set_mac_address(struct net_device *dev, void *addr)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   struct sockaddr *psta_mac_addr = addr;
   eHalStatus halStatus = eHAL_STATUS_SUCCESS;

   ENTER();

   memcpy(&pAdapter->macAddressCurrent, psta_mac_addr->sa_data, ETH_ALEN);

#ifdef HDD_SESSIONIZE 
   // set the MAC address though the STA ID CFG.
   halStatus = ccmCfgSetStr( pAdapter->hHal, WNI_CFG_STA_ID,
                             (v_U8_t *)&pAdapter->macAddressCurrent,
                             sizeof( pAdapter->macAddressCurrent ),
                             hdd_set_mac_addr_cb, VOS_FALSE );
#endif

   memcpy(dev->dev_addr, psta_mac_addr->sa_data, ETH_ALEN);

   EXIT();
   return halStatus;
}

tANI_U8* wlan_hdd_get_intf_addr(hdd_context_t* pHddCtx)
{
   int i;
   for ( i = 0; i < VOS_MAX_CONCURRENCY_PERSONA; i++)
   {
      if( 0 == (pHddCtx->cfg_ini->intfAddrMask >> i))
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

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,29))
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
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,1,0))
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

#endif

void hdd_set_station_ops( struct net_device *pWlanDev )
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,29))
      pWlanDev->tx_queue_len = NET_DEV_TX_QUEUE_LEN,
      pWlanDev->netdev_ops = &wlan_drv_ops;
#else
      pWlanDev->open = hdd_open;
      pWlanDev->stop = hdd_stop;
      pWlanDev->uninit = hdd_uninit;
      pWlanDev->hard_start_xmit = NULL;
      pWlanDev->tx_timeout = hdd_tx_timeout;
      pWlanDev->get_stats = hdd_stats;
      pWlanDev->do_ioctl = hdd_ioctl;
      pWlanDev->tx_queue_len = NET_DEV_TX_QUEUE_LEN;
      pWlanDev->set_mac_address = hdd_set_mac_address;
#endif
}

static hdd_adapter_t* hdd_alloc_station_adapter( hdd_context_t *pHddCtx, tSirMacAddr macAddr, const char* name )
{
   struct net_device *pWlanDev = NULL;
   hdd_adapter_t *pAdapter = NULL;
   /*
    * cfg80211 initialization and registration....
    */ 
   pWlanDev = alloc_netdev_mq(sizeof( hdd_adapter_t ), name, ether_setup, NUM_TX_QUEUES);
   
   if(pWlanDev != NULL)
   {

      //Save the pointer to the net_device in the HDD adapter
      pAdapter = (hdd_adapter_t*) netdev_priv( pWlanDev );

      vos_mem_zero( pAdapter, sizeof( hdd_adapter_t ) );

      pAdapter->dev = pWlanDev;
      pAdapter->pHddCtx = pHddCtx; 
      pAdapter->magic = WLAN_HDD_ADAPTER_MAGIC;

      init_completion(&pAdapter->session_open_comp_var);
      init_completion(&pAdapter->session_close_comp_var);
      init_completion(&pAdapter->disconnect_comp_var);
      init_completion(&pAdapter->linkup_event_var);
      init_completion(&pAdapter->cancel_rem_on_chan_var);
      init_completion(&pAdapter->rem_on_chan_ready_event);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
      init_completion(&pAdapter->offchannel_tx_event);
#endif
      init_completion(&pAdapter->tx_action_cnf_event);
#ifdef FEATURE_WLAN_TDLS
      init_completion(&pAdapter->tdls_add_station_comp);
      init_completion(&pAdapter->tdls_del_station_comp);
      init_completion(&pAdapter->tdls_mgmt_comp);
      init_completion(&pAdapter->tdls_link_establish_req_comp);
#endif
      init_completion(&pHddCtx->mc_sus_event_var);
      init_completion(&pHddCtx->tx_sus_event_var);
      init_completion(&pHddCtx->rx_sus_event_var);
      init_completion(&pAdapter->ula_complete);

      pAdapter->isLinkUpSvcNeeded = FALSE; 
      pAdapter->higherDtimTransition = eANI_BOOLEAN_TRUE;
      //Init the net_device structure
      strlcpy(pWlanDev->name, name, IFNAMSIZ);

      vos_mem_copy(pWlanDev->dev_addr, (void *)macAddr, sizeof(tSirMacAddr));
      vos_mem_copy( pAdapter->macAddressCurrent.bytes, macAddr, sizeof(tSirMacAddr));
      pWlanDev->watchdog_timeo = HDD_TX_TIMEOUT;
      pWlanDev->hard_header_len += LIBRA_HW_NEEDED_HEADROOM;

      hdd_set_station_ops( pAdapter->dev );

      pWlanDev->destructor = free_netdev;
      pWlanDev->ieee80211_ptr = &pAdapter->wdev ;
      pAdapter->wdev.wiphy = pHddCtx->wiphy;  
      pAdapter->wdev.netdev =  pWlanDev;
      /* set pWlanDev's parent to underlying device */
      SET_NETDEV_DEV(pWlanDev, pHddCtx->parent_dev);
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

   /* need to make sure all of our scheduled work has completed.
    * This callback is called from MC thread context, so it is safe to
    * to call below flush workqueue API from here.
    */
   flush_scheduled_work();

   /* We can be blocked while waiting for scheduled work to be
    * flushed, and the adapter structure can potentially be freed, in
    * which case the magic will have been reset.  So make sure the
    * magic is still good, and hence the adapter structure is still
    * valid, before signaling completion */
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
   int rc = 0;

   INIT_COMPLETION(pAdapter->session_open_comp_var);
   sme_SetCurrDeviceMode(pHddCtx->hHal, pAdapter->device_mode);
   //Open a SME session for future operation
   halStatus = sme_OpenSession( pHddCtx->hHal, hdd_smeRoamCallback, pAdapter,
         (tANI_U8 *)&pAdapter->macAddressCurrent, &pAdapter->sessionId);
   if ( !HAL_STATUS_SUCCESS( halStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,
             "sme_OpenSession() failed with status code %08d [x%08lx]",
                                                 halStatus, halStatus );
      status = VOS_STATUS_E_FAILURE;
      goto error_sme_open;
   }
   
   //Block on a completion variable. Can't wait forever though.
   rc = wait_for_completion_interruptible_timeout(
                        &pAdapter->session_open_comp_var,
                        msecs_to_jiffies(WLAN_WAIT_TIME_SESSIONOPENCLOSE));
   if (!rc)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,
             "Session is not opened within timeout period code %08d", rc );
      status = VOS_STATUS_E_FAILURE;
      goto error_sme_open;
   }

   // Register wireless extensions
   if( eHAL_STATUS_SUCCESS !=  (halStatus = hdd_register_wext(pWlanDev)))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,
              "hdd_register_wext() failed with status code %08d [x%08lx]",
                                                   halStatus, halStatus );
      status = VOS_STATUS_E_FAILURE;
      goto error_register_wext;
   }
   //Safe to register the hard_start_xmit function again
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,29))
   wlan_drv_ops.ndo_start_xmit = hdd_hard_start_xmit;
#else
   pWlanDev->hard_start_xmit = hdd_hard_start_xmit;
#endif

   //Set the Connection State to Not Connected
   pHddStaCtx->conn_info.connState = eConnectionState_NotConnected;

   //Set the default operation channel
   pHddStaCtx->conn_info.operationChannel = pHddCtx->cfg_ini->OperatingChannel;

   /* Make the default Auth Type as OPEN*/
   pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_OPEN_SYSTEM;

   if( VOS_STATUS_SUCCESS != ( status = hdd_init_tx_rx( pAdapter ) ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,
            "hdd_init_tx_rx() failed with status code %08d [x%08lx]",
                            status, status );
      goto error_init_txrx;
   }

   set_bit(INIT_TX_RX_SUCCESS, &pAdapter->event_flags);

   if( VOS_STATUS_SUCCESS != ( status = hdd_wmm_adapter_init( pAdapter ) ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,
            "hdd_wmm_adapter_init() failed with status code %08d [x%08lx]",
                            status, status );
      goto error_wmm_init;
   }

   set_bit(WMM_INIT_DONE, &pAdapter->event_flags);

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
         //Block on a completion variable. Can't wait forever though.
         wait_for_completion_timeout(
                          &pAdapter->session_close_comp_var,
                          msecs_to_jiffies(WLAN_WAIT_TIME_SESSIONOPENCLOSE));
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
      int rc;
      INIT_COMPLETION(pAdapter->tx_action_cnf_event);
      rc = wait_for_completion_interruptible_timeout(
                     &pAdapter->tx_action_cnf_event,
                     msecs_to_jiffies(ACTION_FRAME_TX_TIMEOUT));
      if(!rc)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              ("ERROR: HDD Wait for Action Confirmation Failed!!\n"));
      }
   }
   return;
}

void hdd_deinit_adapter( hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter )
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
#ifdef FEATURE_WLAN_TDLS
         if(test_bit(TDLS_INIT_DONE, &pAdapter->event_flags))
         {
            wlan_hdd_tdls_exit(pAdapter);
            clear_bit(TDLS_INIT_DONE, &pAdapter->event_flags);
         }
#endif

         break;
      }

      case WLAN_HDD_SOFTAP:
      case WLAN_HDD_P2P_GO:
      {
         hdd_cleanup_actionframe(pHddCtx, pAdapter);

         hdd_unregister_hostapd(pAdapter);
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

void hdd_cleanup_adapter( hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter, tANI_U8 rtnl_held )
{
   struct net_device *pWlanDev = pAdapter->dev;

   if(test_bit(NET_DEVICE_REGISTERED, &pAdapter->event_flags)) {
      if( rtnl_held )
      {
         unregister_netdevice(pWlanDev);
      }
      else
      {
         unregister_netdev(pWlanDev);
      }
      // note that the pAdapter is no longer valid at this point
      // since the memory has been reclaimed
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
                    hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Fail to Disable Power Save\n", __func__);
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
                    hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Fail to Stop Auto Bmps Timer\n", __func__);
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
                    //Block on a completion variable. Can't wait forever though
                    wait_for_completion_interruptible_timeout(
                         &pHddCtx->full_pwr_comp_var, msecs_to_jiffies(1000));
                 }
                 else
                 {
                    status = VOS_STATUS_E_FAILURE;
                    hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Request for Full Power failed\n", __func__);
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

hdd_adapter_t* hdd_open_adapter( hdd_context_t *pHddCtx, tANI_U8 session_type,
                                 const char *iface_name, tSirMacAddr macAddr,
                                 tANI_U8 rtnl_held )
{
   hdd_adapter_t *pAdapter = NULL;
   hdd_adapter_list_node_t *pHddAdapterNode = NULL;
   VOS_STATUS status = VOS_STATUS_E_FAILURE;
   VOS_STATUS exitbmpsStatus;

   hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s iface =%s type = %d\n",__func__,iface_name,session_type);

   //Disable BMPS incase of Concurrency
   exitbmpsStatus = hdd_disable_bmps_imps(pHddCtx, session_type);

   if(VOS_STATUS_E_FAILURE == exitbmpsStatus)
   {
      //Fail to Exit BMPS
      VOS_ASSERT(0);
      return NULL;
   }

   switch(session_type)
   {
      case WLAN_HDD_INFRA_STATION:
      case WLAN_HDD_P2P_CLIENT:
      case WLAN_HDD_P2P_DEVICE:
      {
         pAdapter = hdd_alloc_station_adapter( pHddCtx, macAddr, iface_name );

         if( NULL == pAdapter )
            return NULL;

         pAdapter->wdev.iftype = (session_type == WLAN_HDD_P2P_CLIENT) ?
                                  NL80211_IFTYPE_P2P_CLIENT:
                                  NL80211_IFTYPE_STATION;

         pAdapter->device_mode = session_type;

         status = hdd_init_station_mode( pAdapter );
         if( VOS_STATUS_SUCCESS != status )
            goto err_free_netdev;

         status = hdd_register_interface( pAdapter, rtnl_held );
         if( VOS_STATUS_SUCCESS != status )
         {
            hdd_deinit_adapter(pHddCtx, pAdapter);
            goto err_free_netdev;
         }
         //Stop the Interface TX queue.
         netif_tx_disable(pAdapter->dev);
         //netif_tx_disable(pWlanDev);
         netif_carrier_off(pAdapter->dev);

         break;
      }

      case WLAN_HDD_P2P_GO:
      case WLAN_HDD_SOFTAP:
      {
         pAdapter = hdd_wlan_create_ap_dev( pHddCtx, macAddr, (tANI_U8 *)iface_name );
         if( NULL == pAdapter )
            return NULL;

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
            hdd_deinit_adapter(pHddCtx, pAdapter);
            goto err_free_netdev;
         }

         netif_tx_disable(pAdapter->dev);
         netif_carrier_off(pAdapter->dev);

         hdd_set_conparam( 1 );
         break;
      }
      case WLAN_HDD_MONITOR:
      {
         pAdapter = hdd_alloc_station_adapter( pHddCtx, macAddr, iface_name );
         if( NULL == pAdapter )
            return NULL;

         pAdapter->wdev.iftype = NL80211_IFTYPE_MONITOR; 
         pAdapter->device_mode = session_type;
         status = hdd_register_interface( pAdapter, rtnl_held );
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,29)
         pAdapter->dev->netdev_ops = &wlan_mon_drv_ops;
#else
         pAdapter->dev->open = hdd_mon_open;
         pAdapter->dev->hard_start_xmit = hdd_mon_hard_start_xmit;
#endif
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
         /* This workqueue will be used to transmit management packet over
          * monitor interface. */
         if (NULL == pAdapter->sessionCtx.monitor.pAdapterForTx) {
             hddLog(VOS_TRACE_LEVEL_ERROR,"%s:Failed:hdd_get_adapter",__func__);
             return NULL;
         }

         INIT_WORK(&pAdapter->sessionCtx.monitor.pAdapterForTx->monTxWorkQueue,
                   hdd_mon_tx_work_queue);
      }
         break;
      case WLAN_HDD_FTM:
      {
         pAdapter = hdd_alloc_station_adapter( pHddCtx, macAddr, iface_name );

         if( NULL == pAdapter )
            return NULL;
         /* Assign NL80211_IFTYPE_STATION as interface type to resolve Kernel Warning
          * message while loading driver in FTM mode. */
         pAdapter->wdev.iftype = NL80211_IFTYPE_STATION;
         pAdapter->device_mode = session_type;
         status = hdd_register_interface( pAdapter, rtnl_held );
      }
         break;
      default:
      {
         VOS_ASSERT(0);
         return NULL;
      }
   }


   if( VOS_STATUS_SUCCESS == status )
   {
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
   }
   return pAdapter;

err_free_netdev:
   free_netdev(pAdapter->dev);
   wlan_hdd_release_intf_addr( pHddCtx,
                               pAdapter->macAddressCurrent.bytes );

resume_bmps:
   //If bmps disabled enable it
   if(VOS_STATUS_SUCCESS == exitbmpsStatus)
   {
       if (pHddCtx->hdd_wlan_suspended)
       {
           hdd_set_pwrparams(pHddCtx);
       }
       hdd_enable_bmps_imps(pHddCtx);
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
      return status;

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


      /* If there is a single session of STA/P2P client, re-enable BMPS */
      if ((!vos_concurrent_sessions_running()) && 
           ((pHddCtx->no_of_sessions[VOS_STA_MODE] >= 1) || 
           (pHddCtx->no_of_sessions[VOS_P2P_CLIENT_MODE] >= 1)))
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
    v_U8_t addIE[1] = {0};

    if ( eHAL_STATUS_FAILURE == ccmCfgSetStr((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
                            WNI_CFG_PROBE_RSP_ADDNIE_DATA1,(tANI_U8*)addIE, 0, NULL,
                            eANI_BOOLEAN_FALSE) )
    {
        hddLog(LOGE,
           "Could not pass on WNI_CFG_PROBE_RSP_ADDNIE_DATA1 to CCM\n");
    }

    if ( eHAL_STATUS_FAILURE == ccmCfgSetStr((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
                            WNI_CFG_PROBE_RSP_ADDNIE_DATA2, (tANI_U8*)addIE, 0, NULL,
                            eANI_BOOLEAN_FALSE) )
    {
        hddLog(LOGE,
           "Could not pass on WNI_CFG_PROBE_RSP_ADDNIE_DATA2 to CCM\n");
    }

    if ( eHAL_STATUS_FAILURE == ccmCfgSetStr((WLAN_HDD_GET_CTX(pHostapdAdapter))->hHal,
                            WNI_CFG_PROBE_RSP_ADDNIE_DATA3, (tANI_U8*)addIE, 0, NULL,
                            eANI_BOOLEAN_FALSE) )
    {
        hddLog(LOGE,
           "Could not pass on WNI_CFG_PROBE_RSP_ADDNIE_DATA3 to CCM\n");
    }
}

VOS_STATUS hdd_stop_adapter( hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter )
{
   eHalStatus halStatus = eHAL_STATUS_SUCCESS;
   hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
   union iwreq_data wrqu;

   ENTER();

   switch(pAdapter->device_mode)
   {
      case WLAN_HDD_INFRA_STATION:
      case WLAN_HDD_P2P_CLIENT:
      case WLAN_HDD_P2P_DEVICE:
         if( hdd_connIsConnected( WLAN_HDD_GET_STATION_CTX_PTR( pAdapter )) )
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
               wait_for_completion_interruptible_timeout(&pAdapter->disconnect_comp_var,
               msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
            }
            memset(&wrqu, '\0', sizeof(wrqu));
            wrqu.ap_addr.sa_family = ARPHRD_ETHER;
            memset(wrqu.ap_addr.sa_data,'\0',ETH_ALEN);
            wireless_send_event(pAdapter->dev, SIOCGIWAP, &wrqu, NULL);
         }
         else
         {
            hdd_abort_mac_scan(pHddCtx);
         }

         if (test_bit(SME_SESSION_OPENED, &pAdapter->event_flags)) 
         {
            INIT_COMPLETION(pAdapter->session_close_comp_var);
            if (eHAL_STATUS_SUCCESS ==
                sme_CloseSession(pHddCtx->hHal, pAdapter->sessionId, 
                                 hdd_smeCloseSessionCallback, pAdapter))
            {
               //Block on a completion variable. Can't wait forever though.
               wait_for_completion_timeout(
                          &pAdapter->session_close_comp_var, 
                          msecs_to_jiffies(WLAN_WAIT_TIME_SESSIONOPENCLOSE));
            }
         }

         break;

      case WLAN_HDD_SOFTAP:
      case WLAN_HDD_P2P_GO:
         //Any softap specific cleanup here...
         mutex_lock(&pHddCtx->sap_lock);
         if (test_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags)) 
         {
            VOS_STATUS status;
            hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

            //Stop Bss.
            status = WLANSAP_StopBss(pHddCtx->pvosContext);
            if (VOS_IS_STATUS_SUCCESS(status))
            {
               hdd_hostapd_state_t *pHostapdState = 
                  WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);

               status = vos_wait_single_event(&pHostapdState->vosEvent, 10000);
   
               if (!VOS_IS_STATUS_SUCCESS(status))
               {
                  hddLog(LOGE, "%s: failure waiting for WLANSAP_StopBss",
                         __func__);
               }
            }
            else
            {
               hddLog(LOGE, "%s: failure in WLANSAP_StopBss", __func__);
            }
            clear_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags);

            if (eHAL_STATUS_FAILURE ==
                ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_PROBE_RSP_BCN_ADDNIE_FLAG,
                             0, NULL, eANI_BOOLEAN_FALSE))
            {
               hddLog(LOGE,
                      "%s: Failed to set WNI_CFG_PROBE_RSP_BCN_ADDNIE_FLAG",
                      __func__);
            }

            if ( eHAL_STATUS_FAILURE == ccmCfgSetInt((WLAN_HDD_GET_CTX(pAdapter))->hHal,
                     WNI_CFG_ASSOC_RSP_ADDNIE_FLAG, 0, NULL,
                     eANI_BOOLEAN_FALSE) )
            {
               hddLog(LOGE,
                     "Could not pass on WNI_CFG_ASSOC_RSP_ADDNIE_FLAG to CCM");
            }

            // Reset WNI_CFG_PROBE_RSP Flags
            wlan_hdd_reset_prob_rspies(pAdapter);
            kfree(pAdapter->sessionCtx.ap.beacon);
            pAdapter->sessionCtx.ap.beacon = NULL;
         }
         mutex_unlock(&pHddCtx->sap_lock);
         break;
      case WLAN_HDD_MONITOR:
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
      netif_tx_disable(pAdapter->dev);
      netif_carrier_off(pAdapter->dev);

      hdd_stop_adapter( pHddCtx, pAdapter );

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
      netif_tx_disable(pAdapter->dev);
      netif_carrier_off(pAdapter->dev);

      pAdapter->sessionCtx.station.hdd_ReassocScenario = VOS_FALSE;

      hdd_deinit_tx_rx(pAdapter);
      hdd_wmm_adapter_close(pAdapter);

      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

   EXIT();

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_start_all_adapters( hdd_context_t *pHddCtx )
{
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   hdd_adapter_t      *pAdapter;
   v_MACADDR_t  bcastMac = VOS_MAC_ADDR_BROADCAST_INITIALIZER;
   eConnectionState  connState;

   ENTER();

   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
      pAdapter = pAdapterNode->pAdapter;

      switch(pAdapter->device_mode)
      {
         case WLAN_HDD_INFRA_STATION:
         case WLAN_HDD_P2P_CLIENT:
         case WLAN_HDD_P2P_DEVICE:

            connState = (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState;

            hdd_init_station_mode(pAdapter);
            /* Open the gates for HDD to receive Wext commands */
            pAdapter->isLinkUpSvcNeeded = FALSE; 
            pHddCtx->scan_info.mScanPending = FALSE;
            pHddCtx->scan_info.waitScanResult = FALSE;

            //Trigger the initial scan
            hdd_wlan_initial_scan(pAdapter);

            //Indicate disconnect event to supplicant if associated previously
            if (eConnectionState_Associated == connState ||
                eConnectionState_IbssConnected == connState )
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
               cfg80211_connect_result(pAdapter->dev, NULL,
                                       NULL, 0, NULL, 0,
                                       WLAN_STATUS_ASSOC_DENIED_UNSPEC,
                                       GFP_KERNEL);
            }
            break;

         case WLAN_HDD_SOFTAP:
            /* softAP can handle SSR */
            break;

         case WLAN_HDD_P2P_GO:
              hddLog(VOS_TRACE_LEVEL_ERROR, "%s [SSR] send restart supplicant",
                                                       __func__);
              /* event supplicant to restart */
              cfg80211_del_sta(pAdapter->dev,
                        (const u8 *)&bcastMac.bytes[0], GFP_KERNEL);
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

         pHddStaCtx->conn_info.connState = eConnectionState_NotConnected;
         init_completion(&pAdapter->disconnect_comp_var);
         sme_RoamDisconnect(pHddCtx->hHal, pAdapter->sessionId,
                             eCSR_DISCONNECT_REASON_UNSPECIFIED);

         wait_for_completion_interruptible_timeout(
                                &pAdapter->disconnect_comp_var,
                                msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));

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
   tCsrBssid staBssid = { 0 }, p2pBssid = { 0 }, apBssid = { 0 };
   v_U8_t staChannel = 0, p2pChannel = 0, apChannel = 0;
   const char *p2pMode = "DEV";
   const char *ccMode = "Standalone";
   int n;

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
          }
          break;
      case WLAN_HDD_P2P_CLIENT:
          pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
          if (eConnectionState_Associated == pHddStaCtx->conn_info.connState) {
              p2pChannel = pHddStaCtx->conn_info.operationChannel;
              memcpy(p2pBssid, pHddStaCtx->conn_info.bssId, sizeof(p2pBssid));
              p2pMode = "CLI";
          }
          break;
      case WLAN_HDD_P2P_GO:
          pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);
          pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);
          if (pHostapdState->bssState == BSS_START && pHostapdState->vosStatus==VOS_STATUS_SUCCESS) {
              p2pChannel = pHddApCtx->operatingChannel;
              memcpy(p2pBssid, pAdapter->macAddressCurrent.bytes, sizeof(p2pBssid));
          }
          p2pMode = "GO";
          break;
      case WLAN_HDD_SOFTAP:
          pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);
          pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);
          if (pHostapdState->bssState == BSS_START && pHostapdState->vosStatus==VOS_STATUS_SUCCESS) {
              apChannel = pHddApCtx->operatingChannel;
              memcpy(apBssid, pAdapter->macAddressCurrent.bytes, sizeof(apBssid));
          }
          break;
      default:
          break;
      }
      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }
   if (staChannel > 0 && (apChannel > 0 || p2pChannel > 0)) {
       ccMode = (p2pChannel==staChannel||apChannel==staChannel) ? "SCC" : "MCC";
   }
   n = pr_info("wlan(%d) " MAC_ADDRESS_STR " %s",
                staChannel, MAC_ADDR_ARRAY(staBssid), ccMode);
   if (p2pChannel > 0) {
       n +=  pr_info("p2p-%s(%d) " MAC_ADDRESS_STR,
                     p2pMode, p2pChannel, MAC_ADDR_ARRAY(p2pBssid));
   }
   if (apChannel > 0) {
       n += pr_info("AP(%d) " MAC_ADDRESS_STR,
                     apChannel, MAC_ADDR_ARRAY(apBssid));
   }

   if (p2pChannel > 0 && apChannel > 0) {
        hddLog(VOS_TRACE_LEVEL_ERROR, "Error concurrent SAP %d and P2P %d which is not support", apChannel, p2pChannel);
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
    spin_lock(&pHddCtx->hddAdapters.lock);
    status =  hdd_list_peek_front ( &pHddCtx->hddAdapters,
                   (hdd_list_node_t**) ppAdapterNode );
    spin_unlock(&pHddCtx->hddAdapters.lock);
    return status;
}

VOS_STATUS hdd_get_next_adapter( hdd_context_t *pHddCtx,
                                 hdd_adapter_list_node_t* pAdapterNode,
                                 hdd_adapter_list_node_t** pNextAdapterNode)
{
    VOS_STATUS status;
    spin_lock(&pHddCtx->hddAdapters.lock);
    status = hdd_list_peek_next ( &pHddCtx->hddAdapters,
                                  (hdd_list_node_t*) pAdapterNode,
                                  (hdd_list_node_t**)pNextAdapterNode );

    spin_unlock(&pHddCtx->hddAdapters.lock);
    return status;
}

VOS_STATUS hdd_remove_adapter( hdd_context_t *pHddCtx,
                               hdd_adapter_list_node_t* pAdapterNode)
{
    VOS_STATUS status;
    spin_lock(&pHddCtx->hddAdapters.lock);
    status =  hdd_list_remove_node ( &pHddCtx->hddAdapters,
                                     &pAdapterNode->node );
    spin_unlock(&pHddCtx->hddAdapters.lock);
    return status;
}

VOS_STATUS hdd_remove_front_adapter( hdd_context_t *pHddCtx,
                                     hdd_adapter_list_node_t** ppAdapterNode)
{
    VOS_STATUS status;
    spin_lock(&pHddCtx->hddAdapters.lock);
    status =  hdd_list_remove_front( &pHddCtx->hddAdapters,
                   (hdd_list_node_t**) ppAdapterNode );
    spin_unlock(&pHddCtx->hddAdapters.lock);
    return status;
}

VOS_STATUS hdd_add_adapter_back( hdd_context_t *pHddCtx,
                                 hdd_adapter_list_node_t* pAdapterNode)
{
    VOS_STATUS status;
    spin_lock(&pHddCtx->hddAdapters.lock);
    status =  hdd_list_insert_back ( &pHddCtx->hddAdapters,
                   (hdd_list_node_t*) pAdapterNode );
    spin_unlock(&pHddCtx->hddAdapters.lock);
    return status;
}

VOS_STATUS hdd_add_adapter_front( hdd_context_t *pHddCtx,
                                  hdd_adapter_list_node_t* pAdapterNode)
{
    VOS_STATUS status;
    spin_lock(&pHddCtx->hddAdapters.lock);
    status =  hdd_list_insert_front ( &pHddCtx->hddAdapters,
                   (hdd_list_node_t*) pAdapterNode );
    spin_unlock(&pHddCtx->hddAdapters.lock);
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
  
  \brief hdd_select_queue() - 

   This API returns the operating channel of the requested device mode 
   
  \param  - pHddCtx - Pointer to the HDD context.
              - mode - Device mode for which operating channel is required
                suported modes - WLAN_HDD_INFRA_STATION, WLAN_HDD_P2P_CLIENT
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

#ifdef WLAN_FEATURE_PACKET_FILTERING
/**---------------------------------------------------------------------------

  \brief hdd_set_multicast_list() - 

  This used to set the multicast address list.

  \param  - dev - Pointer to the WLAN device.
  - skb - Pointer to OS packet (sk_buff).
  \return - success/fail 

  --------------------------------------------------------------------------*/
static void hdd_set_multicast_list(struct net_device *dev)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   int mc_count;
   int i = 0;
   struct netdev_hw_addr *ha;

   if (NULL == pAdapter)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
            "%s: Adapter context is Null", __func__);
      return;
   }

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
         memset(&(pAdapter->mc_addr_list.addr[i][0]), 0, ETH_ALEN);
         memcpy(&(pAdapter->mc_addr_list.addr[i][0]), ha->addr, ETH_ALEN);
         hddLog(VOS_TRACE_LEVEL_INFO, "\n%s: mlist[%d] = "MAC_ADDRESS_STR,
               __func__, i, 
               MAC_ADDR_ARRAY(pAdapter->mc_addr_list.addr[i]));
         i++;
      }
   }
   return;
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
v_U16_t hdd_select_queue(struct net_device *dev,
    struct sk_buff *skb)
{
   return hdd_wmm_select_queue(dev, skb);
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
   unsigned long scanId;
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
            return;
         }
         vos_mem_copy(scanReq.ChannelInfo.ChannelList, channelInfo.ChannelList,
            channelInfo.numOfChannels);
         scanReq.ChannelInfo.numOfChannels = channelInfo.numOfChannels;
         vos_mem_free(channelInfo.ChannelList);
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

struct fullPowerContext
{
   struct completion completion;
   unsigned int magic;
};
#define POWER_CONTEXT_MAGIC  0x504F5752   //POWR

/**---------------------------------------------------------------------------

  \brief hdd_full_power_callback() - HDD full power callback function

  This is the function invoked by SME to inform the result of a full power
  request issued by HDD

  \param  - callbackcontext - Pointer to cookie
  \param  - status - result of request

  \return - None

  --------------------------------------------------------------------------*/
static void hdd_full_power_callback(void *callbackContext, eHalStatus status)
{
   struct fullPowerContext *pContext = callbackContext;

   hddLog(VOS_TRACE_LEVEL_INFO,
          "%s: context = %p, status = %d", __func__, pContext, status);

   if (NULL == callbackContext)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Bad param, context [%p]",
             __func__, callbackContext);
      return;
   }

   /* there is a race condition that exists between this callback function
      and the caller since the caller could time out either before or
      while this code is executing.  we'll assume the timeout hasn't
      occurred, but we'll verify that right before we save our work */

   if (POWER_CONTEXT_MAGIC != pContext->magic)
   {
      /* the caller presumably timed out so there is nothing we can do */
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: Invalid context, magic [%08x]",
              __func__, pContext->magic);
      return;
   }

   /* the race is on.  caller could have timed out immediately after
      we verified the magic, but if so, caller will wait a short time
      for us to notify the caller, so the context will stay valid */
   complete(&pContext->completion);
}

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
   hdd_adapter_t* pAdapter;
   struct fullPowerContext powerContext;
   long lrc;

   ENTER();

   if (VOS_FTM_MODE != hdd_get_conparam())
   {
      // Unloading, restart logic is no more required.
      wlan_hdd_restart_deinit(pHddCtx);
   }

   if (VOS_STA_SAP_MODE != hdd_get_conparam())
   {
      if (VOS_FTM_MODE != hdd_get_conparam())
      {
         hdd_adapter_t* pAdapter = hdd_get_adapter(pHddCtx,
                                      WLAN_HDD_INFRA_STATION);
         if (pAdapter == NULL)
            pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_P2P_CLIENT);

         if (pAdapter != NULL)
         {
            wlan_hdd_cfg80211_pre_voss_stop(pAdapter);
            hdd_UnregisterWext(pAdapter->dev);
         }
      }
   }

   if (VOS_FTM_MODE == hdd_get_conparam())
   {
      wlan_hdd_ftm_close(pHddCtx);
      goto free_hdd_ctx;
   }
   //Stop the Interface TX queue.
   //netif_tx_disable(pWlanDev);
   //netif_carrier_off(pWlanDev);

   if (VOS_STA_SAP_MODE == hdd_get_conparam())
   {
      pAdapter = hdd_get_adapter(pHddCtx,
                                   WLAN_HDD_SOFTAP);
   }
   else
   {
      if (VOS_FTM_MODE != hdd_get_conparam())
      {
         pAdapter = hdd_get_adapter(pHddCtx,
                                    WLAN_HDD_INFRA_STATION);
      }
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

   // Cancel any outstanding scan requests.  We are about to close all
   // of our adapters, but an adapter structure is what SME passes back
   // to our callback function.  Hence if there are any outstanding scan
   // requests then there is a race condition between when the adapter
   // is closed and when the callback is invoked.  We try to resolve that
   // race condition here by canceling any outstanding scans before we
   // close the adapters.
   // Note that the scans may be cancelled in an asynchronous manner, so
   // ideally there needs to be some kind of synchronization.  Rather than
   // introduce a new synchronization here, we will utilize the fact that
   // we are about to Request Full Power, and since that is synchronized,
   // the expectation is that by the time Request Full Power has completed,
   // all scans will be cancelled.
   hdd_abort_mac_scan( pHddCtx );

   //Stop the timer if already running
   if (VOS_TIMER_STATE_RUNNING ==
           vos_timer_getCurrentState(&pHddCtx->hdd_p2p_go_conn_is_in_progress))
   {
       vos_timer_stop(&pHddCtx->hdd_p2p_go_conn_is_in_progress);
   }

   // Destroy hdd_p2p_go_conn_is_in_progress timer
   if (!VOS_IS_STATUS_SUCCESS(vos_timer_destroy(
                         &pHddCtx->hdd_p2p_go_conn_is_in_progress)))
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,
           "%s: Cannot deallocate p2p connection timer", __func__);
   }

   //Stop the traffic monitor timer
   if ( VOS_TIMER_STATE_RUNNING ==
                        vos_timer_getCurrentState(&pHddCtx->tx_rx_trafficTmr))
   {
        vos_timer_stop(&pHddCtx->tx_rx_trafficTmr);
   }

   // Destroy the traffic monitor timer
   if (!VOS_IS_STATUS_SUCCESS(vos_timer_destroy(
                         &pHddCtx->tx_rx_trafficTmr)))
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,
           "%s: Cannot deallocate Traffic monitor timer", __func__);
   }

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
         lrc = wait_for_completion_interruptible_timeout(
                                      &powerContext.completion,
                                      msecs_to_jiffies(WLAN_WAIT_TIME_POWER));
         /* either we have a response or we timed out
            either way, first invalidate our magic */
         powerContext.magic = 0;
         if (lrc <= 0)
         {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: %s while requesting full power",
                   __func__, (0 == lrc) ? "timeout" : "interrupt");
            /* there is a race condition such that the callback
               function could be executing at the same time we are. of
               primary concern is if the callback function had already
               verified the "magic" but hasn't yet set the completion
               variable.  Since the completion variable is on our
               stack, we'll delay just a bit to make sure the data is
               still valid if that is the case */
            msleep(50);
         }
      }
      else
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: Request for Full Power failed, status %d",
                __func__, halStatus);
         VOS_ASSERT(0);
         /* continue -- need to clean up as much as possible */
      }
   }

   hdd_debugfs_exit(pHddCtx);

   // Unregister the Net Device Notifier
   unregister_netdevice_notifier(&hdd_netdev_notifier);
   
   hdd_stop_all_adapters( pHddCtx );

#ifdef WLAN_BTAMP_FEATURE
   vosStatus = WLANBAP_Stop(pVosContext);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to stop BAP",__func__);
   }
#endif //WLAN_BTAMP_FEATURE

   //Stop all the modules
   vosStatus = vos_stop( pVosContext );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
         "%s: Failed to stop VOSS",__func__);
      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   }

   //Assert Deep sleep signal now to put Libra HW in lowest power state
   vosStatus = vos_chipAssertDeepSleep( NULL, NULL, NULL );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );

   //Vote off any PMIC voltage supplies
   vos_chipPowerDown(NULL, NULL, NULL);

   vos_chipVoteOffXOBuffer(NULL, NULL, NULL);


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
#ifdef WLAN_OPEN_SOURCE
#ifdef WLAN_FEATURE_HOLD_RX_WAKELOCK
   /* Destroy the wake lock */
   wake_lock_destroy(&pHddCtx->rx_wake_lock);
#endif
   /* Destroy the wake lock */
   wake_lock_destroy(&pHddCtx->sap_wake_lock);
#endif

   //Close VOSS
   //This frees pMac(HAL) context. There should not be any call that requires pMac access after this.
   vos_close(pVosContext);

   //Close Watchdog
   if(pHddCtx->cfg_ini->fIsLogpEnabled)
      vos_watchdog_close(pVosContext);

   //Clean up HDD Nlink Service
   send_btc_nlink_msg(WLAN_MODULE_DOWN_IND, 0);
#ifdef WLAN_KD_READY_NOTIFIER
   nl_srv_exit(pHddCtx->ptt_pid);
#else
   nl_srv_exit();
#endif /* WLAN_KD_READY_NOTIFIER */

   /* Cancel the vote for XO Core ON. 
    * This is done here to ensure there is no race condition since MC, TX and WD threads have
    * exited at this point
    */
   hddLog(VOS_TRACE_LEVEL_WARN, "In module exit: Cancel the vote for XO Core ON"
                                    " when WLAN is turned OFF\n");
   if (vos_chipVoteXOCore(NULL, NULL, NULL, VOS_FALSE) != VOS_STATUS_SUCCESS)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR, "Could not cancel the vote for XO Core ON." 
                                        " Not returning failure."
                                        " Power consumed will be high\n");
   }  

   hdd_close_all_adapters( pHddCtx );


   //Free up dynamically allocated members inside HDD Adapter
   kfree(pHddCtx->cfg_ini);
   pHddCtx->cfg_ini= NULL;

   /* free the power on lock from platform driver */
   if (free_riva_power_on_lock("wlan"))
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to free power on lock",
                                           __func__);
   }

free_hdd_ctx:
   wiphy_unregister(wiphy) ;
   wiphy_free(wiphy) ;
   if (hdd_is_ssr_required())
   {
       /* WDI timeout had happened during unload, so SSR is needed here */
       subsystem_restart("wcnss");
       msleep(5000);
   }
   hdd_set_ssr_required (VOS_FALSE);
}


/**---------------------------------------------------------------------------

  \brief hdd_update_config_from_nv() - Function to update the contents of
         the running configuration with parameters taken from NV storage

  \param  - pHddCtx - Pointer to the HDD global context

  \return - VOS_STATUS_SUCCESS if successful

  --------------------------------------------------------------------------*/
static VOS_STATUS hdd_update_config_from_nv(hdd_context_t* pHddCtx)
{
   v_BOOL_t itemIsValid = VOS_FALSE;
   VOS_STATUS status;
   v_MACADDR_t macFromNV[VOS_MAX_CONCURRENCY_PERSONA];
   v_U8_t      macLoop;

   /*If the NV is valid then get the macaddress from nv else get it from qcom_cfg.ini*/
   status = vos_nv_getValidity(VNV_FIELD_IMAGE, &itemIsValid);
   if(status != VOS_STATUS_SUCCESS)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR," vos_nv_getValidity() failed\n ");
       return VOS_STATUS_E_FAILURE;
   }

   if (itemIsValid == VOS_TRUE) 
   {
        hddLog(VOS_TRACE_LEVEL_INFO_HIGH," Reading the Macaddress from NV\n ");
      status = vos_nv_readMultiMacAddress((v_U8_t *)&macFromNV[0].bytes[0],
                                          VOS_MAX_CONCURRENCY_PERSONA);
        if(status != VOS_STATUS_SUCCESS)
        {
         /* Get MAC from NV fail, not update CFG info
          * INI MAC value will be used for MAC setting */
         hddLog(VOS_TRACE_LEVEL_ERROR," vos_nv_readMacAddress() failed\n ");
            return VOS_STATUS_E_FAILURE;
        }

      /* If first MAC is not valid, treat all others are not valid
       * Then all MACs will be got from ini file */
      if(vos_is_macaddr_zero(&macFromNV[0]))
      {
         /* MAC address in NV file is not configured yet */
         hddLog(VOS_TRACE_LEVEL_WARN, "Invalid MAC in NV file");
         return VOS_STATUS_E_INVAL;
   }

      /* Get MAC address from NV, update CFG info */
      for(macLoop = 0; macLoop < VOS_MAX_CONCURRENCY_PERSONA; macLoop++)
      {
         if(vos_is_macaddr_zero(&macFromNV[macLoop]))
         {
            printk(KERN_ERR "not valid MAC from NV for %d", macLoop);
            /* This MAC is not valid, skip it
             * This MAC will be got from ini file */
         }
         else
         {
            vos_mem_copy((v_U8_t *)&pHddCtx->cfg_ini->intfMacAddr[macLoop].bytes[0],
                         (v_U8_t *)&macFromNV[macLoop].bytes[0],
                   VOS_MAC_ADDR_SIZE);
         }
      }
   }
   else
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "NV ITEM, MAC Not valid");
      return VOS_STATUS_E_FAILURE;
   }


   return VOS_STATUS_SUCCESS;
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
void hdd_prevent_suspend(void)
{
#ifdef WLAN_OPEN_SOURCE
    wake_lock(&wlan_wake_lock);
#else
    wcnss_prevent_suspend();
#endif
}

void hdd_allow_suspend(void)
{
#ifdef WLAN_OPEN_SOURCE
    wake_unlock(&wlan_wake_lock);
#else
    wcnss_allow_suspend();
#endif
}

void hdd_allow_suspend_timeout(v_U32_t timeout)
{
#ifdef WLAN_OPEN_SOURCE
    wake_lock_timeout(&wlan_wake_lock, msecs_to_jiffies(timeout));
#else
    /* Do nothing as there is no API in wcnss for timeout*/
#endif
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

/**---------------------------------------------------------------------------

  \brief hdd_is_5g_supported() - HDD function to know if hardware supports  5GHz

  \param  - pHddCtx - Pointer to the hdd context

  \return -  true if hardware supports 5GHz

  --------------------------------------------------------------------------*/
static boolean hdd_is_5g_supported(hdd_context_t * pHddCtx)
{
   /* If wcnss_wlan_iris_xo_mode() returns WCNSS_XO_48MHZ(1);
    * then hardware support 5Ghz.
   */
   if (WCNSS_XO_48MHZ == wcnss_wlan_iris_xo_mode())
   {
      hddLog(VOS_TRACE_LEVEL_INFO, "%s: Hardware supports 5Ghz", __func__);
      return true;
   }
   else
   {
      hddLog(VOS_TRACE_LEVEL_INFO, "%s: Hardware doesn't supports 5Ghz",
                    __func__);
      return false;
   }
}


/**---------------------------------------------------------------------------

  \brief hdd_wlan_startup() - HDD init function

  This is the driver startup code executed once a WLAN device has been detected

  \param  - dev - Pointer to the underlying device

  \return -  0 for success, < 0 for failure

  --------------------------------------------------------------------------*/

int hdd_wlan_startup(struct device *dev )
{
   VOS_STATUS status;
   hdd_adapter_t *pAdapter = NULL;
   hdd_adapter_t *pP2pAdapter = NULL;
   hdd_context_t *pHddCtx = NULL;
   v_CONTEXT_t pVosContext= NULL;
#ifdef WLAN_BTAMP_FEATURE
   VOS_STATUS vStatus = VOS_STATUS_SUCCESS;
   WLANBAP_ConfigType btAmpConfig;
   hdd_config_t *pConfig;
#endif
   int ret;
   struct wiphy *wiphy;

   ENTER();
   /*
    * cfg80211: wiphy allocation
    */
   wiphy = wlan_hdd_cfg80211_init(sizeof(hdd_context_t)) ;

   if(wiphy == NULL)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: cfg80211 init failed", __func__);
      return -EIO;
   }

   pHddCtx = wiphy_priv(wiphy);

   //Initialize the adapter context to zeros.
   vos_mem_zero(pHddCtx, sizeof( hdd_context_t ));

   pHddCtx->wiphy = wiphy;
   hdd_prevent_suspend();
   pHddCtx->isLoadUnloadInProgress = TRUE;

   vos_set_load_unload_in_progress(VOS_MODULE_ID_VOSS, TRUE);

   /*Get vos context here bcoz vos_open requires it*/
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

   if(pVosContext == NULL)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Failed vos_get_global_context",__func__);
      goto err_free_hdd_context;
   }

   //Save the Global VOSS context in adapter context for future.
   pHddCtx->pvosContext = pVosContext;

   //Save the adapter context in global context for future.
   ((VosContextType*)(pVosContext))->pHDDContext = (v_VOID_t*)pHddCtx;

   pHddCtx->parent_dev = dev;

   init_completion(&pHddCtx->full_pwr_comp_var);
   init_completion(&pHddCtx->standby_comp_var);
   init_completion(&pHddCtx->req_bmps_comp_var);
   init_completion(&pHddCtx->scan_info.scan_req_completion_event);
   init_completion(&pHddCtx->scan_info.abortscan_event_var);
   init_completion(&pHddCtx->driver_crda_req);

   spin_lock_init(&pHddCtx->schedScan_lock);

   hdd_list_init( &pHddCtx->hddAdapters, MAX_NUMBER_OF_ADAPTERS );

   // Load all config first as TL config is needed during vos_open
   pHddCtx->cfg_ini = (hdd_config_t*) kmalloc(sizeof(hdd_config_t), GFP_KERNEL);
   if(pHddCtx->cfg_ini == NULL)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Failed kmalloc hdd_config_t",__func__);
      goto err_free_hdd_context;
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

   /* If SNR Monitoring is enabled, FW has to parse all beacons
    * for calcaluting and storing the average SNR, so set Nth beacon
    * filter to 1 to enable FW to parse all the beaocons
    */
   if (1 == pHddCtx->cfg_ini->fEnableSNRMonitoring)
   {
      /* The log level is deliberately set to WARN as overriding
       * nthBeaconFilter to 1 will increase power cosumption and this
       * might just prove helpful to detect the power issue.
       */
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: Setting pHddCtx->cfg_ini->nthBeaconFilter = 1", __func__);
      pHddCtx->cfg_ini->nthBeaconFilter = 1;
   }
   /*
    * cfg80211: Initialization and registration ...
    */
   if (0 < wlan_hdd_cfg80211_register(dev, wiphy, pHddCtx->cfg_ini))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, 
              "%s: wlan_hdd_cfg80211_register return failure", __func__);
      goto err_wiphy_reg;
   }

   // Update VOS trace levels based upon the cfg.ini
   hdd_vos_trace_enable(VOS_MODULE_ID_BAP,
                        pHddCtx->cfg_ini->vosTraceEnableBAP);
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

   // Update WDI trace levels based upon the cfg.ini
   hdd_wdi_trace_enable(eWLAN_MODULE_DAL,
                        pHddCtx->cfg_ini->wdiTraceEnableDAL);
   hdd_wdi_trace_enable(eWLAN_MODULE_DAL_CTRL,
                        pHddCtx->cfg_ini->wdiTraceEnableCTL);
   hdd_wdi_trace_enable(eWLAN_MODULE_DAL_DATA,
                        pHddCtx->cfg_ini->wdiTraceEnableDAT);
   hdd_wdi_trace_enable(eWLAN_MODULE_PAL,
                        pHddCtx->cfg_ini->wdiTraceEnablePAL);

   if (VOS_FTM_MODE == hdd_get_conparam())
   {
      if ( VOS_STATUS_SUCCESS != wlan_hdd_ftm_open(pHddCtx) )
      {
          hddLog(VOS_TRACE_LEVEL_FATAL,"%s: wlan_hdd_ftm_open Failed",__func__);
          goto err_free_hdd_context;
      }
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: FTM driver loaded success fully",__func__);
      return VOS_STATUS_SUCCESS;
   }

   //Open watchdog module
   if(pHddCtx->cfg_ini->fIsLogpEnabled)
   {
      status = vos_watchdog_open(pVosContext,
         &((VosContextType*)pVosContext)->vosWatchdog, sizeof(VosWatchdogContext));

      if(!VOS_IS_STATUS_SUCCESS( status ))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_watchdog_open failed",__func__);
         goto err_wiphy_reg;
      }
   }

   pHddCtx->isLogpInProgress = FALSE;
   vos_set_logp_in_progress(VOS_MODULE_ID_VOSS, FALSE);

   status = vos_chipVoteOnXOBuffer(NULL, NULL, NULL);
   if(!VOS_IS_STATUS_SUCCESS(status))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Failed to configure 19.2 MHz Clock", __func__);
      goto err_wdclose;
   }

   status = vos_open( &pVosContext, 0);
   if ( !VOS_IS_STATUS_SUCCESS( status ))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: vos_open failed", __func__);
      goto err_clkvote;
   }

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

   /* Note that the vos_preStart() sequence triggers the cfg download.
      The cfg download must occur before we update the SME config
      since the SME config operation must access the cfg database */
   status = hdd_set_sme_config( pHddCtx );

   if ( VOS_STATUS_SUCCESS != status )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Failed hdd_set_sme_config", __func__);
      goto err_vosclose;
   }

   //Initialize the WMM module
   status = hdd_wmm_init(pHddCtx);
   if (!VOS_IS_STATUS_SUCCESS(status))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: hdd_wmm_init failed", __func__);
      goto err_vosclose;
   }

   /* In the integrated architecture we update the configuration from
      the INI file and from NV before vOSS has been started so that
      the final contents are available to send down to the cCPU   */

   // Apply the cfg.ini to cfg.dat
   if (FALSE == hdd_update_config_dat(pHddCtx))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: config update failed",__func__ );
      goto err_vosclose;
   }

   // Apply the NV to cfg.dat
   /* Prima Update MAC address only at here */
   if (VOS_STATUS_SUCCESS != hdd_update_config_from_nv(pHddCtx))
   {
#ifdef WLAN_AUTOGEN_MACADDR_FEATURE
      /* There was not a valid set of MAC Addresses in NV.  See if the
         default addresses were modified by the cfg.ini settings.  If so,
         we'll use them, but if not, we'll autogenerate a set of MAC
         addresses based upon the device serial number */

      static const v_MACADDR_t default_address =
         {{0x00, 0x0A, 0xF5, 0x89, 0x89, 0xFF}};
      unsigned int serialno;
      int i;

      serialno = wcnss_get_serial_number();
      if ((0 != serialno) &&
          (0 == memcmp(&default_address, &pHddCtx->cfg_ini->intfMacAddr[0],
                       sizeof(default_address))))
      {
         /* cfg.ini has the default address, invoke autogen logic */

         /* MAC address has 3 bytes of OUI so we have a maximum of 3
            bytes of the serial number that can be used to generate
            the other 3 bytes of the MAC address.  Mask off all but
            the lower 3 bytes (this will also make sure we don't
            overflow in the next step) */
         serialno &= 0x00FFFFFF;

         /* we need a unique address for each session */
         serialno *= VOS_MAX_CONCURRENCY_PERSONA;

         /* autogen all addresses */
         for (i = 0; i < VOS_MAX_CONCURRENCY_PERSONA; i++)
         {
            /* start with the entire default address */
            pHddCtx->cfg_ini->intfMacAddr[i] = default_address;
            /* then replace the lower 3 bytes */
            pHddCtx->cfg_ini->intfMacAddr[i].bytes[3] = (serialno >> 16) & 0xFF;
            pHddCtx->cfg_ini->intfMacAddr[i].bytes[4] = (serialno >> 8) & 0xFF;
            pHddCtx->cfg_ini->intfMacAddr[i].bytes[5] = serialno & 0xFF;

            serialno++;
         }

         pr_info("wlan: Invalid MAC addresses in NV, autogenerated "
                MAC_ADDRESS_STR,
                MAC_ADDR_ARRAY(pHddCtx->cfg_ini->intfMacAddr[0].bytes));
      }
      else
#endif //WLAN_AUTOGEN_MACADDR_FEATURE
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: Invalid MAC address in NV, using MAC from ini file "
                MAC_ADDRESS_STR, __func__,
                MAC_ADDR_ARRAY(pHddCtx->cfg_ini->intfMacAddr[0].bytes));
      }
   }
   {
      eHalStatus halStatus;
      // Set the MAC Address
      // Currently this is used by HAL to add self sta. Remove this once self sta is added as part of session open.
      halStatus = cfgSetStr( pHddCtx->hHal, WNI_CFG_STA_ID,
                             (v_U8_t *)&pHddCtx->cfg_ini->intfMacAddr[0],
                             sizeof( pHddCtx->cfg_ini->intfMacAddr[0]) );
   
      if (!HAL_STATUS_SUCCESS( halStatus ))
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Failed to set MAC Address. "
                "HALStatus is %08d [x%08x]",__func__, halStatus, halStatus );
         goto err_vosclose;
      }
   }

   /*Start VOSS which starts up the SME/MAC/HAL modules and everything else
     Note: Firmware image will be read and downloaded inside vos_start API */
   status = vos_start( pHddCtx->pvosContext );
   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_start failed",__func__);
      goto err_vosclose;
   }

   /* Exchange capability info between Host and FW and also get versioning info from FW */
   hdd_exchange_version_and_caps(pHddCtx);

   status = hdd_post_voss_start_config( pHddCtx );
   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hdd_post_voss_start_config failed", 
         __func__);
      goto err_vosstop;
   }

   if (VOS_STA_SAP_MODE == hdd_get_conparam())
   {
     pAdapter = hdd_open_adapter( pHddCtx, WLAN_HDD_SOFTAP, "softap.%d", 
         wlan_hdd_get_intf_addr(pHddCtx), FALSE );
   }
   else
   {
     pAdapter = hdd_open_adapter( pHddCtx, WLAN_HDD_INFRA_STATION, "wlan%d",
         wlan_hdd_get_intf_addr(pHddCtx), FALSE );
     if (pAdapter != NULL)
     {
         if ( pHddCtx->cfg_ini->isP2pDeviceAddrAdministrated )
         {
               vos_mem_copy( pHddCtx->p2pDeviceAddress.bytes,
                       pHddCtx->cfg_ini->intfMacAddr[0].bytes,
                       sizeof(tSirMacAddr));

                /* Generate the P2P Device Address.  This consists of the device's
                 * primary MAC address with the locally administered bit set.
                */
                pHddCtx->p2pDeviceAddress.bytes[0] |= 0x02;
         }
         else
         {
             tANI_U8* p2p_dev_addr = wlan_hdd_get_intf_addr(pHddCtx);
             if (p2p_dev_addr != NULL)
             {
                 vos_mem_copy(&pHddCtx->p2pDeviceAddress.bytes[0],
                             p2p_dev_addr, VOS_MAC_ADDR_SIZE);
             }
             else
             {
                   hddLog(VOS_TRACE_LEVEL_FATAL,
                           "%s: Failed to allocate mac_address for p2p_device",
                   __func__);
                   goto err_close_adapter;
             }
         }

         pP2pAdapter = hdd_open_adapter( pHddCtx, WLAN_HDD_P2P_DEVICE, "p2p%d",
                           &pHddCtx->p2pDeviceAddress.bytes[0], FALSE );
         if ( NULL == pP2pAdapter )
         {
             hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to do hdd_open_adapter for P2P Device Interface",
                __func__);
             goto err_close_adapter;
         }
     }
   }

   if( pAdapter == NULL )
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: hdd_open_adapter failed", __func__);
      goto err_close_adapter;
   }

#ifdef WLAN_BTAMP_FEATURE
   vStatus = WLANBAP_Open(pVosContext);
   if(!VOS_IS_STATUS_SUCCESS(vStatus))
   {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: Failed to open BAP",__func__);
      goto err_close_adapter;
   }

   vStatus = BSL_Init(pVosContext);
   if(!VOS_IS_STATUS_SUCCESS(vStatus))
   {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: Failed to Init BSL",__func__);
     goto err_bap_close;
   }
   vStatus = WLANBAP_Start(pVosContext);
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to start TL",__func__);
       goto err_bap_close;
   }

   pConfig = pHddCtx->cfg_ini;
   btAmpConfig.ucPreferredChannel = pConfig->preferredChannel;
   status = WLANBAP_SetConfig(&btAmpConfig);

#endif //WLAN_BTAMP_FEATURE

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
   /*SME must send channel update configuration to RIVA*/
   sme_UpdateChannelConfig(pHddCtx->hHal); 
#endif

   /* Register with platform driver as client for Suspend/Resume */
   status = hddRegisterPmOps(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hddRegisterPmOps failed",__func__);
#ifdef WLAN_BTAMP_FEATURE
      goto err_bap_stop;
#else
      goto err_close_adapter; 
#endif //WLAN_BTAMP_FEATURE
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

   // register net device notifier for device change notification
   ret = register_netdevice_notifier(&hdd_netdev_notifier);

   if(ret < 0)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: register_netdevice_notifier failed",__func__);
      goto err_free_power_on_lock;
   }

   //Initialize the nlink service
   if(nl_srv_init() != 0)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: nl_srv_init failed", __func__);
      goto err_reg_netdev;
   }

   //Initialize the BTC service
   if(btc_activate_service(pHddCtx) != 0)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: btc_activate_service failed",__func__);
      goto err_nl_srv;
   }

#ifdef PTT_SOCK_SVC_ENABLE
   //Initialize the PTT service
   if(ptt_sock_activate_svc(pHddCtx) != 0)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: ptt_sock_activate_svc failed",__func__);
      goto err_nl_srv;
   }
#endif

   hdd_register_mcast_bcast_filter(pHddCtx);
   if (VOS_STA_SAP_MODE != hdd_get_conparam())
   {
      /* Action frame registered in one adapter which will
       * applicable to all interfaces 
       */
      wlan_hdd_cfg80211_post_voss_start(pAdapter);
   }

   mutex_init(&pHddCtx->sap_lock);

   pHddCtx->isLoadUnloadInProgress = FALSE;

#ifdef WLAN_OPEN_SOURCE
#ifdef WLAN_FEATURE_HOLD_RX_WAKELOCK
   /* Initialize the wake lcok */
   wake_lock_init(&pHddCtx->rx_wake_lock,
           WAKE_LOCK_SUSPEND,
           "qcom_rx_wakelock");
#endif
   /* Initialize the wake lcok */
   wake_lock_init(&pHddCtx->sap_wake_lock,
           WAKE_LOCK_SUSPEND,
           "qcom_sap_wakelock");
#endif

   vos_event_init(&pHddCtx->scan_info.scan_finished_event);
   pHddCtx->scan_info.scan_pending_option = WEXT_SCAN_PENDING_GIVEUP;

   vos_set_load_unload_in_progress(VOS_MODULE_ID_VOSS, FALSE);
   hdd_allow_suspend();

   // Initialize the restart logic
   wlan_hdd_restart_init(pHddCtx);

   if (!VOS_IS_STATUS_SUCCESS( vos_timer_init( &pHddCtx->hdd_p2p_go_conn_is_in_progress,
         VOS_TIMER_TYPE_SW, wlan_hdd_p2p_go_connection_in_progresscb, pAdapter) ) )
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,
           "%s: vos timer init failed for hdd_p2p_go_conn_is_in_progress", __func__);
   }

   //Register the traffic monitor timer now
   if ( pHddCtx->cfg_ini->dynSplitscan)
   {
       vos_timer_init(&pHddCtx->tx_rx_trafficTmr,
                     VOS_TIMER_TYPE_SW,
                     hdd_tx_rx_pkt_cnt_stat_timer_handler,
                     (void *)pHddCtx);
   }
   goto success;

err_nl_srv:
#ifdef WLAN_KD_READY_NOTIFIER
   nl_srv_exit(pHddCtx->ptt_pid);
#else
   nl_srv_exit();
#endif /* WLAN_KD_READY_NOTIFIER */
err_reg_netdev:
   unregister_netdevice_notifier(&hdd_netdev_notifier);

err_free_power_on_lock:
   free_riva_power_on_lock("wlan");

err_unregister_pmops:
   hddDevTmUnregisterNotifyCallback(pHddCtx);
   hddDeregisterPmOps(pHddCtx);

   hdd_debugfs_exit(pHddCtx);

#ifdef WLAN_BTAMP_FEATURE
err_bap_stop:
  WLANBAP_Stop(pVosContext);
#endif

#ifdef WLAN_BTAMP_FEATURE
err_bap_close:
   WLANBAP_Close(pVosContext);
#endif

err_close_adapter:
   hdd_close_all_adapters( pHddCtx );

err_vosstop:
   vos_stop(pVosContext);

err_vosclose:    
   status = vos_sched_close( pVosContext );
   if (!VOS_IS_STATUS_SUCCESS(status))    {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
         "%s: Failed to close VOSS Scheduler", __func__);
      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( status ) );
   }
   vos_close(pVosContext ); 

err_clkvote:
   vos_chipVoteOffXOBuffer(NULL, NULL, NULL);

err_wdclose:
   if(pHddCtx->cfg_ini->fIsLogpEnabled)
      vos_watchdog_close(pVosContext);

err_wiphy_reg:
   wiphy_unregister(wiphy) ; 

err_config:
   kfree(pHddCtx->cfg_ini);
   pHddCtx->cfg_ini= NULL;

err_free_hdd_context:
   hdd_allow_suspend();
   wiphy_free(wiphy) ;
   //kfree(wdev) ;
   VOS_BUG(1);

   if (hdd_is_ssr_required())
   {
       /* WDI timeout had happened during load, so SSR is needed here */
       subsystem_restart("wcnss");
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

   This is the driver entry point - called in different timeline depending
   on whether the driver is statically or dynamically linked

  \param  - None

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/
static int hdd_driver_init( void)
{
   VOS_STATUS status;
   v_CONTEXT_t pVosContext = NULL;
   struct device *dev = NULL;
   int ret_status = 0;
#ifdef HAVE_WCNSS_CAL_DOWNLOAD
   int max_retries = 0;
#endif

#ifdef WCONN_TRACE_KMSG_LOG_BUFF
   vos_wconn_trace_init();
#endif

   ENTER();

#ifdef WLAN_OPEN_SOURCE
   wake_lock_init(&wlan_wake_lock, WAKE_LOCK_SUSPEND, "wlan");
#endif

   pr_info("%s: loading driver v%s\n", WLAN_MODULE_NAME,
           QWLAN_VERSIONSTR TIMER_MANAGER_STR MEMORY_DEBUG_STR);

   //Power Up Libra WLAN card first if not already powered up
   status = vos_chipPowerUp(NULL,NULL,NULL);
   if (!VOS_IS_STATUS_SUCCESS(status))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Libra WLAN not Powered Up. "
          "exiting", __func__);
#ifdef WLAN_OPEN_SOURCE
      wake_lock_destroy(&wlan_wake_lock);
#endif
      return -EIO;
   }

#ifdef ANI_BUS_TYPE_PCI

   dev = wcnss_wlan_get_device();

#endif // ANI_BUS_TYPE_PCI

#ifdef ANI_BUS_TYPE_PLATFORM

#ifdef HAVE_WCNSS_CAL_DOWNLOAD
   /* wait until WCNSS driver downloads NV */
   while (!wcnss_device_ready() && 5 >= ++max_retries) {
       msleep(1000);
   }
   if (max_retries >= 5) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: WCNSS driver not ready", __func__);
#ifdef WLAN_OPEN_SOURCE
      wake_lock_destroy(&wlan_wake_lock);
#endif
      return -ENODEV;
   }
#endif

   dev = wcnss_wlan_get_device();
#endif // ANI_BUS_TYPE_PLATFORM


   do {
      if (NULL == dev) {
         hddLog(VOS_TRACE_LEVEL_FATAL, "%s: WLAN device not found!!",__func__);
         ret_status = -1;
         break;
   }

#ifdef MEMORY_DEBUG
      vos_mem_init();
#endif

#ifdef TIMER_MANAGER
      vos_timer_manager_init();
#endif

      /* Preopen VOSS so that it is ready to start at least SAL */
      status = vos_preOpen(&pVosContext);

   if (!VOS_IS_STATUS_SUCCESS(status))
   {
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Failed to preOpen VOSS", __func__);
         ret_status = -1;
         break;
   }

#ifndef MODULE
      /* For statically linked driver, call hdd_set_conparam to update curr_con_mode
       */
      hdd_set_conparam((v_UINT_t)con_mode);
#endif

      // Call our main init function
      if (hdd_wlan_startup(dev))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s: WLAN Driver Initialization failed",
                __func__);
         vos_preClose( &pVosContext );
         ret_status = -1;
         break;
      }

      /* Cancel the vote for XO Core ON
       * This is done here for safety purposes in case we re-initialize without turning
       * it OFF in any error scenario.
       */
      hddLog(VOS_TRACE_LEVEL_INFO, "In module init: Ensure Force XO Core is OFF"
                                       " when  WLAN is turned ON so Core toggles"
                                       " unless we enter PSD");
      if (vos_chipVoteXOCore(NULL, NULL, NULL, VOS_FALSE) != VOS_STATUS_SUCCESS)
      {
          hddLog(VOS_TRACE_LEVEL_ERROR, "Could not cancel XO Core ON vote. Not returning failure."
                                            " Power consumed will be high\n");
      }
   } while (0);

   if (0 != ret_status)
   {
      //Assert Deep sleep signal now to put Libra HW in lowest power state
      status = vos_chipAssertDeepSleep( NULL, NULL, NULL );
      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( status) );

      //Vote off any PMIC voltage supplies
      vos_chipPowerDown(NULL, NULL, NULL);
#ifdef TIMER_MANAGER
      vos_timer_exit();
#endif
#ifdef MEMORY_DEBUG
      vos_mem_exit();
#endif

#ifdef WLAN_OPEN_SOURCE
      wake_lock_destroy(&wlan_wake_lock);
#endif
      pr_err("%s: driver load failure\n", WLAN_MODULE_NAME);
   }
   else
   {
      //Send WLAN UP indication to Nlink Service
      send_btc_nlink_msg(WLAN_MODULE_UP_IND, 0);

      pr_info("%s: driver loaded\n", WLAN_MODULE_NAME);
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
  or con_mode was changed by userspace)

  \param  - None

  \return - None

  --------------------------------------------------------------------------*/
static void hdd_driver_exit(void)
{
   hdd_context_t *pHddCtx = NULL;
   v_CONTEXT_t pVosContext = NULL;
   int retry = 0;

   pr_info("%s: unloading driver v%s\n", WLAN_MODULE_NAME, QWLAN_VERSIONSTR);

   //Get the global vos context
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

   if(!pVosContext)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Global VOS context is Null", __func__);
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
      while(isWDresetInProgress()) {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
              "%s:SSR in Progress; block rmmod for 1 second!!!", __func__);
         msleep(1000);

         if (retry++ == HDD_MOD_EXIT_SSR_MAX_RETRIES) {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
              "%s:SSR never completed, fatal error", __func__);
            VOS_BUG(0);
         }
      }


      pHddCtx->isLoadUnloadInProgress = TRUE;
      vos_set_load_unload_in_progress(VOS_MODULE_ID_VOSS, TRUE);

      //Do all the cleanup before deregistering the driver
      hdd_wlan_exit(pHddCtx);
   }

   vos_preClose( &pVosContext );

#ifdef TIMER_MANAGER
   vos_timer_exit();
#endif
#ifdef MEMORY_DEBUG
   vos_mem_exit();
#endif

#ifdef WCONN_TRACE_KMSG_LOG_BUFF
   vos_wconn_trace_exit();
#endif

done:
#ifdef WLAN_OPEN_SOURCE
   wake_lock_destroy(&wlan_wake_lock);
#endif
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

static int con_mode_handler(const char *kmessage,
                                 struct kernel_param *kp)
{
   return param_set_int(kmessage, kp);
}
#else /* #ifdef MODULE */
/**---------------------------------------------------------------------------

  \brief kickstart_driver

   This is the driver entry point
   - delayed driver initialization when driver is statically linked
   - invoked when module parameter fwpath is modified from userspace to signal
     initializing the WLAN driver or when con_mode is modified from userspace
     to signal a switch in operating mode

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/
static int kickstart_driver(void)
{
   int ret_status;

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
      ret = kickstart_driver();
   return ret;
}

/**---------------------------------------------------------------------------

  \brief con_mode_handler() -

  Handler function for module param con_mode when it is changed by userspace
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
      ret = kickstart_driver();
   return ret;
}
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

VOS_STATUS hdd_softap_sta_deauth(hdd_adapter_t *pAdapter, v_U8_t *pDestMacAddress)
{
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pAdapter))->pvosContext;
    VOS_STATUS vosStatus = VOS_STATUS_E_FAULT;

    ENTER();

    hddLog(LOG1, "hdd_softap_sta_deauth:(%p, false)",
           (WLAN_HDD_GET_CTX(pAdapter))->pvosContext);

    //Ignore request to deauth bcmc station
    if( pDestMacAddress[0] & 0x1 )
       return vosStatus;

    vosStatus = WLANSAP_DeauthSta(pVosContext,pDestMacAddress);

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

void hdd_softap_sta_disassoc(hdd_adapter_t *pAdapter,v_U8_t *pDestMacAddress)
{
        v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pAdapter))->pvosContext;

    ENTER();

    hddLog( LOGE, "hdd_softap_sta_disassoc:(%p, false)", (WLAN_HDD_GET_CTX(pAdapter))->pvosContext);

    //Ignore request to disassoc bcmc station
    if( pDestMacAddress[0] & 0x1 )
       return;

    WLANSAP_DisassocSta(pVosContext,pDestMacAddress);
}

void hdd_softap_tkip_mic_fail_counter_measure(hdd_adapter_t *pAdapter,v_BOOL_t enable)
{
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pAdapter))->pvosContext;

    ENTER();

    hddLog( LOGE, "hdd_softap_tkip_mic_fail_counter_measure:(%p, false)", (WLAN_HDD_GET_CTX(pAdapter))->pvosContext);

    WLANSAP_SetCounterMeasure(pVosContext, (v_BOOL_t)enable);
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
          return (tVOS_CONCURRENCY_MODE)pHddCtx->concurrency_mode;
       }
    }

    /* we are in an invalid state :( */
    hddLog(LOGE, "%s: Invalid context", __func__);
    return VOS_STA;
}

/* Decide whether to allow/not the apps power collapse. 
 * Allow apps power collapse if we are in connected state.
 * if not, allow only if we are in IMPS  */
v_BOOL_t hdd_is_apps_power_collapse_allowed(hdd_context_t* pHddCtx)
{
    tPmcState pmcState = pmcGetPmcState(pHddCtx->hHal);
    tANI_BOOLEAN scanRspPending = csrNeighborRoamScanRspPending(pHddCtx->hHal);
    tANI_BOOLEAN inMiddleOfRoaming = csrNeighborMiddleOfRoaming(pHddCtx->hHal);
    hdd_config_t *pConfig = pHddCtx->cfg_ini;
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL; 
    hdd_adapter_t *pAdapter = NULL; 
    VOS_STATUS status;
    tVOS_CONCURRENCY_MODE concurrent_state = 0;

    if (VOS_STA_SAP_MODE == hdd_get_conparam())
        return TRUE;

    concurrent_state = hdd_get_concurrency_mode();

#ifdef WLAN_ACTIVEMODE_OFFLOAD_FEATURE
    if(((concurrent_state == (VOS_STA | VOS_P2P_CLIENT)) || 
        (concurrent_state == (VOS_STA | VOS_P2P_GO))) && 
        (IS_ACTIVEMODE_OFFLOAD_FEATURE_ENABLE))
        return TRUE;
#endif

    /*loop through all adapters. TBD fix for Concurrency */
    status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
    while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
    {
        pAdapter = pAdapterNode->pAdapter;
        if ( (WLAN_HDD_INFRA_STATION == pAdapter->device_mode)
          || (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode) )
        {
            if (((pConfig->fIsImpsEnabled || pConfig->fIsBmpsEnabled)
                 && (pmcState != IMPS && pmcState != BMPS
                  &&  pmcState != STOPPED && pmcState != STANDBY)) ||
                 (eANI_BOOLEAN_TRUE == scanRspPending) ||
                 (eANI_BOOLEAN_TRUE == inMiddleOfRoaming))
            {
                hddLog( LOGE, "%s: do not allow APPS power collapse-"
                    "pmcState = %d scanRspPending = %d inMiddleOfRoaming = %d",
                    __func__, pmcState, scanRspPending, inMiddleOfRoaming );
                return FALSE;
            }
        }
        status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
        pAdapterNode = pNext;
    }
    return TRUE;
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
   switch(mode)
   {
       case VOS_STA_MODE:
       case VOS_P2P_CLIENT_MODE:
       case VOS_P2P_GO_MODE:
       case VOS_STA_SAP_MODE:
            pHddCtx->concurrency_mode |= (1 << mode);
            pHddCtx->no_of_sessions[mode]++;
            break;
       default:
            break;

   }
   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: concurrency_mode = 0x%x NumberofSessions for mode %d = %d",
    __func__,pHddCtx->concurrency_mode,mode,pHddCtx->no_of_sessions[mode]);
}


void wlan_hdd_clear_concurrency_mode(hdd_context_t *pHddCtx, tVOS_CON_MODE mode)
{
   switch(mode)
   {
       case VOS_STA_MODE:
       case VOS_P2P_CLIENT_MODE:
       case VOS_P2P_GO_MODE:
       case VOS_STA_SAP_MODE:
    pHddCtx->no_of_sessions[mode]--;
    if (!(pHddCtx->no_of_sessions[mode]))
            pHddCtx->concurrency_mode &= (~(1 << mode));
            break;
       default:
            break;
   }
   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: concurrency_mode = 0x%x NumberofSessions for mode %d = %d",
    __func__,pHddCtx->concurrency_mode,mode,pHddCtx->no_of_sessions[mode]);
}

/**---------------------------------------------------------------------------
 *
 *   \brief wlan_hdd_restart_init
 *
 *   This function initalizes restart timer/flag. An internal function.
 *
 *   \param  - pHddCtx
 *
 *   \return - None
 *             
 * --------------------------------------------------------------------------*/

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
/**---------------------------------------------------------------------------
 *
 *   \brief wlan_hdd_restart_deinit
 *
 *   This function cleans up the resources used. An internal function.
 *
 *   \param  - pHddCtx
 *
 *   \return - None
 *             
 * --------------------------------------------------------------------------*/

static void wlan_hdd_restart_deinit(hdd_context_t* pHddCtx)
{
 
   VOS_STATUS vos_status;
   /* Block any further calls */
   atomic_set(&pHddCtx->isRestartInProgress, 1);
   /* Cleanup */
   vos_status = vos_timer_stop( &pHddCtx->hdd_restart_timer );
   if (!VOS_IS_STATUS_SUCCESS(vos_status))
          hddLog(LOGW, FL("Failed to stop HDD restart timer"));
   vos_status = vos_timer_destroy(&pHddCtx->hdd_restart_timer);
   if (!VOS_IS_STATUS_SUCCESS(vos_status))
          hddLog(LOGW, FL("Failed to destroy HDD restart timer"));

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
   
   /* Prepare the DEAUTH managment frame with reason code */
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
   do 
   {
      if( (status == VOS_STATUS_SUCCESS) && 
                           pAdapterNode  && 
                           pAdapterNode->pAdapter)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, 
               "restarting the driver(intf:\'%s\' mode:%d :try %d)",
               pAdapterNode->pAdapter->dev->name,
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
         
         cfg80211_send_unprot_deauth(pAdapterNode->pAdapter->dev, (u_int8_t*)mgmt, len );  
      }
      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   } while((NULL != pAdapterNode) && (VOS_STATUS_SUCCESS == status));


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

//Register the module init/exit functions
module_init(hdd_module_init);
module_exit(hdd_module_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Qualcomm Atheros, Inc.");
MODULE_DESCRIPTION("WLAN HOST DEVICE DRIVER");

module_param_call(con_mode, con_mode_handler, param_get_int, &con_mode,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

module_param_call(fwpath, fwpath_changed_handler, param_get_string, &fwpath,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
