/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

/**=============================================================================
*     wlan_hdd_early_suspend.c
*
*     \brief      power management functions
*
*     Description

*
==============================================================================**/
/* $HEADER$ */

/**-----------------------------------------------------------------------------
*   Include files
* ----------------------------------------------------------------------------*/

#include <linux/pm.h>
#include <linux/wait.h>
#include <linux/cpu.h>
#include <wlan_hdd_includes.h>
#include <wlan_qct_driver.h>
#if defined(WLAN_OPEN_SOURCE) && defined(CONFIG_HAS_WAKELOCK)
#include <linux/wakelock.h>
#endif
#include "halTypes.h"
#include "sme_Api.h"
#include <vos_api.h>
#include <vos_sched.h>
#include <macInitApi.h>
#include <wlan_qct_sys.h>
#include <wlan_btc_svc.h>
#include <wlan_nlink_common.h>
#include <wlan_hdd_main.h>
#include <wlan_hdd_assoc.h>
#include <wlan_hdd_dev_pwr.h>
#include <wlan_nlink_srv.h>
#include <wlan_hdd_misc.h>
#include <dbglog_host.h>

#include <linux/semaphore.h>
#include <wlan_hdd_hostapd.h>
#include "cfgApi.h"


#include <wcnss_api.h>
#include <linux/inetdevice.h>
#include <wlan_hdd_cfg.h>
#include <wlan_hdd_cfg80211.h>
#include <net/addrconf.h>
#ifdef IPA_OFFLOAD
#include <wlan_hdd_ipa.h>
#endif

/**-----------------------------------------------------------------------------
*   Preprocessor definitions and constants
* ----------------------------------------------------------------------------*/

/**-----------------------------------------------------------------------------
*   Type declarations
* ----------------------------------------------------------------------------*/

/**-----------------------------------------------------------------------------
*   Function and variables declarations
* ----------------------------------------------------------------------------*/
#include "wlan_hdd_power.h"
#include "wlan_hdd_packet_filtering.h"

#include <wlan_qct_wda.h>
#if defined(HIF_PCI)
#include "if_pci.h"
#elif defined(HIF_USB)
#include "if_usb.h"
#elif defined(HIF_SDIO)
#include "if_ath_sdio.h"
#endif

#include "ol_fw.h"
/* Time in msec */
#ifdef CONFIG_SLUB_DEBUG_ON
#define HDD_SSR_BRING_UP_TIME 40000
#else
#define HDD_SSR_BRING_UP_TIME 30000
#endif

static eHalStatus g_full_pwr_status;
static eHalStatus g_standby_status;

extern VOS_STATUS hdd_post_voss_start_config(hdd_context_t* pHddCtx);
extern void hdd_wlan_initial_scan(hdd_context_t *pHddCtx);

extern struct notifier_block hdd_netdev_notifier;
extern tVOS_CON_MODE hdd_get_conparam ( void );

static struct timer_list ssr_timer;
static bool ssr_timer_started;

//Callback invoked by PMC to report status of standby request
void hdd_suspend_standby_cbk (void *callbackContext, eHalStatus status)
{
   hdd_context_t *pHddCtx = (hdd_context_t*)callbackContext;
   hddLog(VOS_TRACE_LEVEL_INFO, "%s: Standby status = %d", __func__, status);
   g_standby_status = status;

   if(eHAL_STATUS_SUCCESS == status)
   {
      pHddCtx->hdd_ps_state = eHDD_SUSPEND_STANDBY;
   }
   else
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestStandby failed",__func__);
   }

   complete(&pHddCtx->standby_comp_var);
}

//Callback invoked by PMC to report status of full power request
void hdd_suspend_full_pwr_callback(void *callbackContext, eHalStatus status)
{
   hdd_context_t *pHddCtx = (hdd_context_t*)callbackContext;
   hddLog(VOS_TRACE_LEVEL_INFO, "%s: Full Power status = %d", __func__, status);
   g_full_pwr_status = status;

   if(eHAL_STATUS_SUCCESS == status)
   {
      pHddCtx->hdd_ps_state = eHDD_SUSPEND_NONE;
   }
   else
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestFullPower failed",__func__);
   }

   complete(&pHddCtx->full_pwr_comp_var);
}

eHalStatus hdd_exit_standby(hdd_context_t *pHddCtx)
{
    eHalStatus status = VOS_STATUS_SUCCESS;
    unsigned long rc;

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: WLAN being resumed from standby",__func__);
    INIT_COMPLETION(pHddCtx->full_pwr_comp_var);

   g_full_pwr_status = eHAL_STATUS_FAILURE;
    status = sme_RequestFullPower(pHddCtx->hHal, hdd_suspend_full_pwr_callback, pHddCtx,
      eSME_FULL_PWR_NEEDED_BY_HDD);

   if(status == eHAL_STATUS_PMC_PENDING)
   {
      //Block on a completion variable. Can't wait forever though
      rc = wait_for_completion_timeout(
                 &pHddCtx->full_pwr_comp_var,
                 msecs_to_jiffies(WLAN_WAIT_TIME_FULL_PWR));
      if (!rc) {
          hddLog(VOS_TRACE_LEVEL_ERROR,
             FL("wait on full_pwr_comp_var failed"));
      }
      status = g_full_pwr_status;
      if(g_full_pwr_status != eHAL_STATUS_SUCCESS)
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestFullPower failed",__func__);
         VOS_ASSERT(0);
         goto failure;
      }
    }
    else if(status != eHAL_STATUS_SUCCESS)
    {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestFullPower failed - status %d",
         __func__, status);
      VOS_ASSERT(0);
      goto failure;
    }
    else
      pHddCtx->hdd_ps_state = eHDD_SUSPEND_NONE;

failure:
    //No blocking to reduce latency. No other device should be depending on WLAN
    //to finish resume and WLAN won't be instantly on after resume
    return status;
}


//Helper routine to put the chip into standby
VOS_STATUS hdd_enter_standby(hdd_context_t *pHddCtx)
{
   eHalStatus halStatus = eHAL_STATUS_SUCCESS;
   VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
   unsigned long rc;

   //Disable IMPS/BMPS as we do not want the device to enter any power
   //save mode on its own during suspend sequence
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);

   //Note we do not disable queues unnecessarily. Queues should already be disabled
   //if STA is disconnected or the queue will be disabled as and when disconnect
   //happens because of standby procedure.

   //Ensure that device is in full power first. There is scope for optimization
   //here especially in scenarios where PMC is already in IMPS or REQUEST_IMPS.
   //Core s/w needs to be optimized to handle this. Until then we request full
   //power before issuing request for standby.
   INIT_COMPLETION(pHddCtx->full_pwr_comp_var);
   g_full_pwr_status = eHAL_STATUS_FAILURE;
   halStatus = sme_RequestFullPower(pHddCtx->hHal, hdd_suspend_full_pwr_callback,
       pHddCtx, eSME_FULL_PWR_NEEDED_BY_HDD);

   if(halStatus == eHAL_STATUS_PMC_PENDING)
   {
      //Block on a completion variable. Can't wait forever though
      rc = wait_for_completion_timeout(
                 &pHddCtx->full_pwr_comp_var,
                 msecs_to_jiffies(WLAN_WAIT_TIME_FULL_PWR));
      if (!rc) {
          hddLog(VOS_TRACE_LEVEL_ERROR,
                 FL("wait on full_pwr_comp_var failed"));
      }

      if(g_full_pwr_status != eHAL_STATUS_SUCCESS)
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestFullPower Failed",__func__);
         VOS_ASSERT(0);
         vosStatus = VOS_STATUS_E_FAILURE;
         goto failure;
      }
   }
   else if(halStatus != eHAL_STATUS_SUCCESS)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestFullPower failed - status %d",
         __func__, halStatus);
      VOS_ASSERT(0);
      vosStatus = VOS_STATUS_E_FAILURE;
      goto failure;
   }

   if(pHddCtx->hdd_mcastbcast_filter_set == TRUE) {
         hdd_conf_mcastbcast_filter(pHddCtx, FALSE);
         pHddCtx->hdd_mcastbcast_filter_set = FALSE;
   }

   //Request standby. Standby will cause the STA to disassociate first. TX queues
   //will be disabled (by HDD) when STA disconnects. You do not want to disable TX
   //queues here. Also do not assert if the failure code is eHAL_STATUS_PMC_NOT_NOW as PMC
   //will send this failure code in case of concurrent sessions. Power Save cannot be supported
   //when there are concurrent sessions.
   INIT_COMPLETION(pHddCtx->standby_comp_var);
   g_standby_status = eHAL_STATUS_FAILURE;
   halStatus = sme_RequestStandby(pHddCtx->hHal, hdd_suspend_standby_cbk, pHddCtx);

   if (halStatus == eHAL_STATUS_PMC_PENDING)
   {
      //Wait till WLAN device enters standby mode
      rc = wait_for_completion_timeout(&pHddCtx->standby_comp_var,
         msecs_to_jiffies(WLAN_WAIT_TIME_STANDBY));
      if (!rc) {
          hddLog(VOS_TRACE_LEVEL_ERROR,
                 FL("wait on standby_comp_var failed"));
      }

      if (g_standby_status != eHAL_STATUS_SUCCESS && g_standby_status != eHAL_STATUS_PMC_NOT_NOW)
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestStandby failed",__func__);
         VOS_ASSERT(0);
         vosStatus = VOS_STATUS_E_FAILURE;
         goto failure;
      }
   }
   else if (halStatus != eHAL_STATUS_SUCCESS && halStatus != eHAL_STATUS_PMC_NOT_NOW) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestStandby failed - status %d",
         __func__, halStatus);
      VOS_ASSERT(0);
      vosStatus = VOS_STATUS_E_FAILURE;
      goto failure;
   }
   else
      pHddCtx->hdd_ps_state = eHDD_SUSPEND_STANDBY;

failure:
   //Restore IMPS config
   if(pHddCtx->cfg_ini->fIsImpsEnabled)
      sme_EnablePowerSave(pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);

   //Restore BMPS config
   if(pHddCtx->cfg_ini->fIsBmpsEnabled)
      sme_EnablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);

   return vosStatus;
}


//Helper routine for Deep sleep entry
VOS_STATUS hdd_enter_deep_sleep(hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter)
{
   eHalStatus halStatus;
   VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
   unsigned long rc;

   //Stop the Interface TX queue.
   hddLog(LOG1, FL("Disabling queues"));
   netif_tx_disable(pAdapter->dev);
   netif_carrier_off(pAdapter->dev);

   //Disable IMPS,BMPS as we do not want the device to enter any power
   //save mode on it own during suspend sequence
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);

   //Ensure that device is in full power as we will touch H/W during vos_Stop
   INIT_COMPLETION(pHddCtx->full_pwr_comp_var);
   g_full_pwr_status = eHAL_STATUS_FAILURE;
   halStatus = sme_RequestFullPower(pHddCtx->hHal, hdd_suspend_full_pwr_callback,
       pHddCtx, eSME_FULL_PWR_NEEDED_BY_HDD);

   if(halStatus == eHAL_STATUS_PMC_PENDING)
   {
      //Block on a completion variable. Can't wait forever though
      rc = wait_for_completion_timeout(
                 &pHddCtx->full_pwr_comp_var,
                 msecs_to_jiffies(WLAN_WAIT_TIME_FULL_PWR));
      if (!rc) {
          hddLog(VOS_TRACE_LEVEL_ERROR,
              FL("wait on full_pwr_comp_var failed"));
      }

      if(g_full_pwr_status != eHAL_STATUS_SUCCESS){
         hddLog(VOS_TRACE_LEVEL_FATAL,"%s: sme_RequestFullPower failed",__func__);
         VOS_ASSERT(0);
      }
   }
   else if(halStatus != eHAL_STATUS_SUCCESS)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Request for Full Power failed",__func__);
      VOS_ASSERT(0);
   }

   //Issue a disconnect. This is required to inform the supplicant that
   //STA is getting disassociated and for GUI to be updated properly
   INIT_COMPLETION(pAdapter->disconnect_comp_var);
   halStatus = sme_RoamDisconnect(pHddCtx->hHal, pAdapter->sessionId, eCSR_DISCONNECT_REASON_UNSPECIFIED);

   //Success implies disconnect command got queued up successfully
   if(halStatus == eHAL_STATUS_SUCCESS)
   {
      //Block on a completion variable. Can't wait forever though.
      rc = wait_for_completion_timeout(
                 &pAdapter->disconnect_comp_var,
                 msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
      if (!rc) {
          hddLog(VOS_TRACE_LEVEL_ERROR,
             FL("wait on disconnect_comp_var failed"));
      }
   }
   //None of the steps should fail after this. Continue even in case of failure
   vosStatus = vos_stop( pHddCtx->pvosContext );
   if (!VOS_IS_STATUS_SUCCESS( vosStatus ))
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: vos_stop return failed %d",
             __func__, vosStatus);
      VOS_ASSERT(0);
   }

   pHddCtx->hdd_ps_state = eHDD_SUSPEND_DEEP_SLEEP;

   //Restore IMPS config
   if(pHddCtx->cfg_ini->fIsImpsEnabled)
      sme_EnablePowerSave(pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);

   //Restore BMPS config
   if(pHddCtx->cfg_ini->fIsBmpsEnabled)
      sme_EnablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);

   return vosStatus;
}

VOS_STATUS hdd_exit_deep_sleep(hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter)
{
   VOS_STATUS vosStatus;
   eHalStatus halStatus;
   tANI_U32 type, subType;

   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
      "%s: calling hdd_set_sme_config",__func__);
   vosStatus = hdd_set_sme_config( pHddCtx );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed in hdd_set_sme_config",__func__);
      goto err_deep_sleep;
   }

   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
      "%s: calling vos_start",__func__);
   vosStatus = vos_start( pHddCtx->pvosContext );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed in vos_start",__func__);
      goto err_deep_sleep;
   }

   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
      "%s: calling hdd_post_voss_start_config",__func__);
   vosStatus = hdd_post_voss_start_config( pHddCtx );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed in hdd_post_voss_start_config",__func__);
      goto err_voss_stop;
   }

   vosStatus = vos_get_vdev_types(pAdapter->device_mode, &type, &subType);
   if (VOS_STATUS_SUCCESS != vosStatus)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "failed to get vdev type");
      goto err_voss_stop;
   }

   //Open a SME session for future operation
   halStatus = sme_OpenSession( pHddCtx->hHal, hdd_smeRoamCallback, pAdapter,
         (tANI_U8 *)&pAdapter->macAddressCurrent, &pAdapter->sessionId,
         type, subType);
   if ( !HAL_STATUS_SUCCESS( halStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"sme_OpenSession() failed with status code %08d [x%08x]",
                    halStatus, halStatus );
      goto err_voss_stop;

   }

   pHddCtx->hdd_ps_state = eHDD_SUSPEND_NONE;

   //Trigger the initial scan
   hdd_wlan_initial_scan(pHddCtx);

   return VOS_STATUS_SUCCESS;

err_voss_stop:
   vos_stop(pHddCtx->pvosContext);
err_deep_sleep:
   return VOS_STATUS_E_FAILURE;

}

#ifdef WLAN_FEATURE_GTK_OFFLOAD
void hdd_conf_gtk_offload(hdd_adapter_t *pAdapter, v_BOOL_t fenable)
{
    eHalStatus ret;
    tSirGtkOffloadParams hddGtkOffloadReqParams;
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    if(fenable)
    {
        if ((eConnectionState_Associated == pHddStaCtx->conn_info.connState) &&
           (GTK_OFFLOAD_ENABLE == pHddStaCtx->gtkOffloadReqParams.ulFlags ))
        {
            vos_mem_copy(&hddGtkOffloadReqParams,
                 &pHddStaCtx->gtkOffloadReqParams,
                 sizeof (tSirGtkOffloadParams));

            ret = sme_SetGTKOffload(WLAN_HDD_GET_HAL_CTX(pAdapter),
                          &hddGtkOffloadReqParams, pAdapter->sessionId);
            if (eHAL_STATUS_SUCCESS != ret)
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                       "%s: sme_SetGTKOffload failed, returned %d",
                       __func__, ret);
                return;
            }

            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "%s: sme_SetGTKOffload successful", __func__);
        }

    }
    else
    {
        if ((eConnectionState_Associated == pHddStaCtx->conn_info.connState) &&
            (0 ==  memcmp(&pHddStaCtx->gtkOffloadReqParams.bssId,
                     &pHddStaCtx->conn_info.bssId, VOS_MAC_ADDR_SIZE)) &&
            (GTK_OFFLOAD_ENABLE == pHddStaCtx->gtkOffloadReqParams.ulFlags))
        {

            /* Host driver has previously  offloaded GTK rekey  */
            ret = sme_GetGTKOffload(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                wlan_hdd_cfg80211_update_replayCounterCallback,
                                pAdapter, pAdapter->sessionId);
            if (eHAL_STATUS_SUCCESS != ret)

            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                       "%s: sme_GetGTKOffload failed, returned %d",
                       __func__, ret);
                return;
            }
            else
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                       "%s: sme_GetGTKOffload successful",
                       __func__);

                /* Sending GTK offload disable */
                memcpy(&hddGtkOffloadReqParams, &pHddStaCtx->gtkOffloadReqParams,
                      sizeof (tSirGtkOffloadParams));
                hddGtkOffloadReqParams.ulFlags = GTK_OFFLOAD_DISABLE;
                ret = sme_SetGTKOffload(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                &hddGtkOffloadReqParams, pAdapter->sessionId);
                if (eHAL_STATUS_SUCCESS != ret)
                {
                    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                            "%s: failed to disable GTK offload, returned %d",
                            __func__, ret);
                    return;
                }
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                        "%s: successfully disabled GTK offload request to HAL",
                        __func__);
            }
        }
    }
    return;
}
#endif /*WLAN_FEATURE_GTK_OFFLOAD*/

#ifdef WLAN_NS_OFFLOAD

static int __wlan_hdd_ipv6_changed(struct notifier_block *nb,
				   unsigned long data, void *arg)
{
	struct inet6_ifaddr *ifa = (struct inet6_ifaddr *)arg;
	struct net_device *ndev = ifa->idev->dev;
	hdd_context_t *hdd_ctx;
	hdd_adapter_t *adapter;
	int status;

	hdd_ctx = container_of(nb, hdd_context_t, ipv6_notifier);
	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status) {
		hddLog(LOGE, FL("HDD context is invalid"));
		return NOTIFY_DONE;
	}

	adapter = WLAN_HDD_GET_PRIV_PTR(ndev);
	if (WLAN_HDD_ADAPTER_MAGIC != adapter->magic) return NOTIFY_DONE;
	if (adapter->dev != ndev) return NOTIFY_DONE;
	if (WLAN_HDD_GET_CTX(adapter) != hdd_ctx) return NOTIFY_DONE;

	if (adapter->device_mode == WLAN_HDD_INFRA_STATION ||
		(adapter->device_mode == WLAN_HDD_P2P_CLIENT)) {
		if (hdd_ctx->cfg_ini->nEnableSuspend ==
			WLAN_MAP_SUSPEND_TO_MCAST_BCAST_FILTER)
			schedule_work(&adapter->ipv6NotifierWorkQueue);
		else
			hddLog(LOG1, FL("Not scheduling ipv6 wq nEnableSuspend: %d"),
				hdd_ctx->cfg_ini->nEnableSuspend);
	}

	return NOTIFY_DONE;
}

/**
 * wlan_hdd_ipv6_changed() - IPv6 change notifier callback
 * @nb: pointer to notifier block
 * @data: data
 * @arg: arg
 *
 * This is the IPv6 notifier callback function gets invoked
 * if any change in IP and then invoke the function @__wlan_hdd_ipv6_changed
 * to reconfigure the offload parameters.
 *
 * Return: 0 on success, error number otherwise.
 */
int wlan_hdd_ipv6_changed(struct notifier_block *nb,
				unsigned long data, void *arg)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_ipv6_changed(nb, data, arg);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**----------------------------------------------------------------------------

  \brief hdd_conf_ns_offload() - Configure NS offload

  Called during SUSPEND to configure the NS offload (MC BC filter) which
  reduces power consumption.

  \param  - pAdapter - Adapter context for which NS offload is to be configured
  \param  - fenable - 0 - disable.
                      1 - enable. (with IPv6 notifier registration)
                      2 - enable. (without IPv6 notifier registration)

  \return - void

  ---------------------------------------------------------------------------*/
static void hdd_conf_ns_offload(hdd_adapter_t *pAdapter, int fenable)
{
    struct inet6_dev *in6_dev;
    struct inet6_ifaddr *ifp;
    struct list_head *p;
    tANI_U8 selfIPv6Addr[SIR_MAC_NUM_TARGET_IPV6_NS_OFFLOAD_NA][SIR_MAC_IPV6_ADDR_LEN] = {{0,}};
    tANI_BOOLEAN selfIPv6AddrValid[SIR_MAC_NUM_TARGET_IPV6_NS_OFFLOAD_NA] = {0};
    tSirHostOffloadReq offLoadRequest;
    hdd_context_t *pHddCtx;

    int i =0;
    eHalStatus returnStatus;
    uint32_t count = 0, scope;

    ENTER();
    hddLog(LOG1, FL(" fenable = %d"), fenable);

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    /* In SAP/P2PGo mode, ARP/NS offload feature capability
     * is controlled by one bit.
     */

     if ((WLAN_HDD_SOFTAP == pAdapter->device_mode ||
          WLAN_HDD_P2P_GO == pAdapter->device_mode) &&
          !pHddCtx->ap_arpns_support) {
           hddLog(LOG1, FL("NS Offload is not supported in SAP/P2PGO mode"));
           return;
    }

    if (fenable) {
        in6_dev = __in6_dev_get(pAdapter->dev);
        if (NULL != in6_dev) {
            list_for_each(p, &in6_dev->addr_list) {
                if (count >= SIR_MAC_NUM_TARGET_IPV6_NS_OFFLOAD_NA) {
                     hddLog(LOG1, FL("Reached max supported NS Offload addresses"));
                     break;
                }
                ifp = list_entry(p, struct inet6_ifaddr, if_list);
                scope = ipv6_addr_src_scope(&ifp->addr);
                switch (scope) {
                case IPV6_ADDR_SCOPE_GLOBAL:
                case IPV6_ADDR_SCOPE_LINKLOCAL:
                     vos_mem_copy(&selfIPv6Addr[count], &ifp->addr.s6_addr,
                            sizeof(ifp->addr.s6_addr));
                     selfIPv6AddrValid[count] = SIR_IPV6_ADDR_VALID;
                     hddLog (LOG1,
                        FL("Index %d scope = %s Address : %pI6"),
                        count, (scope == IPV6_ADDR_SCOPE_LINKLOCAL) ?
                        "LINK LOCAL": "GLOBAL", selfIPv6Addr[count]);
                     count += 1;
                     break;
                default:
                     hddLog(LOGE, "The Scope %d is not supported",
                          ipv6_addr_src_scope(&ifp->addr));
                }

            }
            vos_mem_zero(&offLoadRequest, sizeof(offLoadRequest));
            for (i = 0; i < count; i++) {
                /* Filling up the request structure
                 * Filling the selfIPv6Addr with solicited address
                 * A Solicited-Node multicast address is created by
                 * taking the last 24 bits of a unicast or anycast
                 * address and appending them to the prefix
                 *
                 * FF02:0000:0000:0000:0000:0001:FFXX:XXXX
                 *
                 * here XX is the unicast/anycast bits
                 */
                offLoadRequest.nsOffloadInfo.selfIPv6Addr[i][0] = 0xFF;
                offLoadRequest.nsOffloadInfo.selfIPv6Addr[i][1] = 0x02;
                offLoadRequest.nsOffloadInfo.selfIPv6Addr[i][11] = 0x01;
                offLoadRequest.nsOffloadInfo.selfIPv6Addr[i][12] = 0xFF;
                offLoadRequest.nsOffloadInfo.selfIPv6Addr[i][13] =
                                                  selfIPv6Addr[i][13];
                offLoadRequest.nsOffloadInfo.selfIPv6Addr[i][14] =
                                                   selfIPv6Addr[i][14];
                offLoadRequest.nsOffloadInfo.selfIPv6Addr[i][15] =
                                                    selfIPv6Addr[i][15];
                offLoadRequest.nsOffloadInfo.slotIdx = i;

                vos_mem_copy(&offLoadRequest.nsOffloadInfo.targetIPv6Addr[i],
                   &selfIPv6Addr[i][0], SIR_MAC_IPV6_ADDR_LEN);

                offLoadRequest.nsOffloadInfo.targetIPv6AddrValid[i] =
                                                    SIR_IPV6_ADDR_VALID;

                hddLog (LOG1,
                    FL("configuredMcastBcastFilter: %d"),
                    pHddCtx->configuredMcastBcastFilter);

                if ((VOS_TRUE == pHddCtx->sus_res_mcastbcast_filter_valid)
                   && ((HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST ==
                     pHddCtx->sus_res_mcastbcast_filter) ||
                     (HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST_BROADCAST ==
                     pHddCtx->sus_res_mcastbcast_filter))) {
                        hddLog(LOG1,
                            FL("Set offLoadRequest with SIR_OFFLOAD_NS_AND_MCAST_FILTER_ENABLE"));
                        offLoadRequest.enableOrDisable =
                            SIR_OFFLOAD_NS_AND_MCAST_FILTER_ENABLE;
                }

                vos_mem_copy(&offLoadRequest.params.hostIpv6Addr,
                   &offLoadRequest.nsOffloadInfo.targetIPv6Addr[i],
                   sizeof(tANI_U8)*SIR_MAC_IPV6_ADDR_LEN);

                hddLog (LOG1,
                   FL("Setting NSOffload with solicitedIp: %pI6, targetIp: %pI6, Index %d"),
                   &offLoadRequest.nsOffloadInfo.selfIPv6Addr[i],
                   &offLoadRequest.nsOffloadInfo.targetIPv6Addr[i], i);
            }
            offLoadRequest.offloadType =  SIR_IPV6_NS_OFFLOAD;
            offLoadRequest.enableOrDisable = SIR_OFFLOAD_ENABLE;
            vos_mem_copy(&offLoadRequest.nsOffloadInfo.selfMacAddr,
               &pAdapter->macAddressCurrent.bytes, SIR_MAC_ADDR_LEN);
            /* set number of ns offload address count */
            offLoadRequest.num_ns_offload_count = count;
            /* Configure the Firmware with this */
            returnStatus = sme_SetHostOffload(WLAN_HDD_GET_HAL_CTX(pAdapter),
                               pAdapter->sessionId, &offLoadRequest);
            if (eHAL_STATUS_SUCCESS != returnStatus) {
                 hddLog(LOGE,
                        FL("Failed to enable HostOffload feature with status: %d"),
                        returnStatus);
            }
        }
        else {
            hddLog(LOGE,
                    FL("IPv6 dev does not exist. Failed to request NSOffload"));
            return;
        }
    } else {
        /* Disable NSOffload */
        vos_mem_zero((void *)&offLoadRequest, sizeof(tSirHostOffloadReq));
        offLoadRequest.enableOrDisable = SIR_OFFLOAD_DISABLE;
        offLoadRequest.offloadType =  SIR_IPV6_NS_OFFLOAD;

        if (eHAL_STATUS_SUCCESS !=
             sme_SetHostOffload(WLAN_HDD_GET_HAL_CTX(pAdapter),
                 pAdapter->sessionId, &offLoadRequest)) {
             hddLog(LOGE, FL("Failed to disable NS Offload"));
        }
    }
    EXIT();
    return;
}

/**
 * __hdd_ipv6_notifier_work_queue() - IP V6 change notifier work handler
 * @work: Pointer to work context
 *
 * Return: none
 */
static void __hdd_ipv6_notifier_work_queue(struct work_struct *work)
{
    hdd_adapter_t* pAdapter =
             container_of(work, hdd_adapter_t, ipv6NotifierWorkQueue);
    hdd_context_t *pHddCtx;
    int status;

    ENTER();

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
        return;

    if ( VOS_FALSE == pHddCtx->sus_res_mcastbcast_filter_valid)
    {
        pHddCtx->sus_res_mcastbcast_filter =
            pHddCtx->configuredMcastBcastFilter;
        pHddCtx->sus_res_mcastbcast_filter_valid = VOS_TRUE;
    }

    if ((eConnectionState_Associated ==
                (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState)
        && (pHddCtx->hdd_wlan_suspended))
    {
        /*
         * This invocation being part of the IPv6 registration callback,
         * we are passing second parameter as 2 to avoid registration
         * of IPv6 notifier again
         */
        if (pHddCtx->cfg_ini->fhostNSOffload)
            hdd_conf_ns_offload(pAdapter, 2);
    }
    EXIT();
}

/**
 * hdd_ipv6_notifier_work_queue() - IP V6 change notifier work handler
 * @work: Pointer to work context
 *
 * Return: none
 */
void hdd_ipv6_notifier_work_queue(struct work_struct *work)
{
	vos_ssr_protect(__func__);
	__hdd_ipv6_notifier_work_queue(work);
	vos_ssr_unprotect(__func__);
}


#endif

/*
 * Function: hdd_conf_hostoffload
 *           Central function to configure the supported offloads,
 *           either enable or disable them.
 */
void hdd_conf_hostoffload(hdd_adapter_t *pAdapter, v_BOOL_t fenable)
{
    hdd_context_t *pHddCtx = NULL;
    v_CONTEXT_t *pVosContext = NULL;
    VOS_STATUS vstatus = VOS_STATUS_E_FAILURE;

    ENTER();

    hddLog(VOS_TRACE_LEVEL_INFO, FL("Configuring offloads with flag: %d"),
            fenable);

    pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

    if (NULL == pVosContext)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL(" Global VOS context is Null"));
        return;
    }

    //Get the HDD context.
    pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext );

    if (NULL == pHddCtx)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: HDD context is Null", __func__);
        return;
    }

    if ((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) ||
           (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode) ||
               (WLAN_HDD_SOFTAP == pAdapter->device_mode) ||
               (WLAN_HDD_P2P_GO == pAdapter->device_mode))
    {
        if (fenable)
        {
            if ((eConnectionState_Associated ==
                    (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState) ||
                (WLAN_HDD_SOFTAP == pAdapter->device_mode) ||
                (WLAN_HDD_P2P_GO == pAdapter->device_mode))
            {
                if ((pHddCtx->cfg_ini->fhostArpOffload))
                {
                    /*
                     * Configure the ARP Offload.
                     * Even if it fails we have to reconfigure the MC/BC
                     * filter flag as we want RIVA not to drop BroadCast
                     * Packets
                     */
                    hddLog(VOS_TRACE_LEVEL_INFO,
                            FL("Calling ARP Offload with flag: %d"), fenable);
                    vstatus = hdd_conf_arp_offload(pAdapter, fenable);
                    pHddCtx->configuredMcastBcastFilter &=
                            ~(HDD_MCASTBCASTFILTER_FILTER_ALL_BROADCAST);

                    if (!VOS_IS_STATUS_SUCCESS(vstatus))
                    {
                        hddLog(VOS_TRACE_LEVEL_ERROR,
                                "Failed to enable ARPOFfloadFeature %d",
                                vstatus);
                    }
                }
                //Configure GTK_OFFLOAD
#ifdef WLAN_FEATURE_GTK_OFFLOAD
                hdd_conf_gtk_offload(pAdapter, fenable);
#endif

#ifdef WLAN_NS_OFFLOAD
                if (pHddCtx->cfg_ini->fhostNSOffload)
                {
                    /*
                     * Configure the NS Offload.
                     * Even if it fails we have to reconfigure the MC/BC filter flag
                     * as we want RIVA not to drop Multicast Packets
                     */

                    hddLog(VOS_TRACE_LEVEL_INFO,
                            FL("Calling NS Offload with flag: %d"), fenable);
                    hdd_conf_ns_offload(pAdapter, fenable);
                    pHddCtx->configuredMcastBcastFilter &=
                            ~(HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST);
                }

#endif
                /*
                * This variable saves the state if offload were configured
                * or not. helps in recovering when pcie fails to suspend
                * because of ongoing scan and state is no longer associated.
                */
                pAdapter->offloads_configured = TRUE;
            }
        }
        else
        {
            //Disable ARPOFFLOAD
            if ( (eConnectionState_Associated ==
                 (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState) ||
                 (pAdapter->offloads_configured == TRUE)
               )
            {
                pAdapter->offloads_configured = FALSE;

                if (pHddCtx->cfg_ini->fhostArpOffload)
                {
                    vstatus = hdd_conf_arp_offload(pAdapter, fenable);
                    if (!VOS_IS_STATUS_SUCCESS(vstatus))
                    {
                        hddLog(VOS_TRACE_LEVEL_ERROR,
                             "Failed to disable ARPOffload Feature %d", vstatus);
                    }
                }
               //Disable GTK_OFFLOAD
#ifdef WLAN_FEATURE_GTK_OFFLOAD
                hdd_conf_gtk_offload(pAdapter, fenable);
#endif

#ifdef WLAN_NS_OFFLOAD
                //Disable NSOFFLOAD
                if (pHddCtx->cfg_ini->fhostNSOffload)
                {
                    hdd_conf_ns_offload(pAdapter, fenable);
                }
#endif
            }
        }
    }
    EXIT();
    return;
}

/**
 * __hdd_ipv4_notifier_work_queue() - IP V4 change notifier work handler
 * @work: Pointer to work context
 *
 * Return: none
 */
static void __hdd_ipv4_notifier_work_queue(struct work_struct *work)
{
    hdd_adapter_t* pAdapter =
             container_of(work, hdd_adapter_t, ipv4NotifierWorkQueue);
    hdd_context_t *pHddCtx;
    int status;

    hddLog(LOG1, FL("Reconfiguring ARP Offload"));
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(LOGE, FL("HDD context is invalid"));
        return;
    }

    if ( VOS_FALSE == pHddCtx->sus_res_mcastbcast_filter_valid)
    {
        pHddCtx->sus_res_mcastbcast_filter =
            pHddCtx->configuredMcastBcastFilter;
        pHddCtx->sus_res_mcastbcast_filter_valid = VOS_TRUE;
    }

    if ((eConnectionState_Associated ==
                (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState)
        && (pHddCtx->hdd_wlan_suspended))
    {
        // This invocation being part of the IPv4 registration callback,
        // we are passing second parameter as 2 to avoid registration
        // of IPv4 notifier again.
        hdd_conf_arp_offload(pAdapter, 2);
    }
}

/**
 * hdd_ipv4_notifier_work_queue() - IP V4 change notifier work handler
 * @work: Pointer to work context
 *
 * Return: none
 */
void hdd_ipv4_notifier_work_queue(struct work_struct *work)
{
	vos_ssr_protect(__func__);
	__hdd_ipv4_notifier_work_queue(work);
	vos_ssr_unprotect(__func__);
}

static int __wlan_hdd_ipv4_changed(struct notifier_block *nb,
				   unsigned long data, void *arg)
{
	struct in_ifaddr *ifa = (struct in_ifaddr *)arg;
	struct in_ifaddr **ifap = NULL;
	struct in_device *in_dev;
	struct net_device *ndev = ifa->ifa_dev->dev;
	hdd_context_t *hdd_ctx;
	hdd_adapter_t *adapter;
	int status;

	hdd_ctx = container_of(nb, hdd_context_t, ipv4_notifier);
	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status) {
		hddLog(LOGE, FL("HDD context is invalid"));
		return NOTIFY_DONE;
	}

	adapter = WLAN_HDD_GET_PRIV_PTR(ndev);
	if (!adapter) return NOTIFY_DONE;
	if (WLAN_HDD_ADAPTER_MAGIC != adapter->magic) return NOTIFY_DONE;
	if (adapter->dev != ndev) return NOTIFY_DONE;
	if (WLAN_HDD_GET_CTX(adapter) != hdd_ctx) return NOTIFY_DONE;
        if (!(adapter->device_mode == WLAN_HDD_INFRA_STATION ||
                adapter->device_mode == WLAN_HDD_P2P_CLIENT))
		return NOTIFY_DONE;

	if ((hdd_ctx->cfg_ini->nEnableSuspend !=
				WLAN_MAP_SUSPEND_TO_MCAST_BCAST_FILTER) ||
			(!hdd_ctx->cfg_ini->fhostArpOffload)) {
		hddLog(LOG1, FL("Offload not enabled MCBC=%d, ARPOffload=%d"),
				hdd_ctx->cfg_ini->nEnableSuspend,
				hdd_ctx->cfg_ini->fhostArpOffload);

		return NOTIFY_DONE;
	}

	in_dev = __in_dev_get_rtnl(adapter->dev);
	if (in_dev != NULL) {
		for (ifap = &in_dev->ifa_list;
			(ifa = *ifap) != NULL;
			ifap = &ifa->ifa_next) {
			if (!strcmp(adapter->dev->name,
				ifa->ifa_label))
				break; /* found */
		}
	}

	if (ifa && ifa->ifa_local)
		schedule_work(&adapter->ipv4NotifierWorkQueue);

	return NOTIFY_DONE;
}

/**
 * wlan_hdd_ipv4_changed() - IPv4 change notifier callback
 * @nb: pointer to notifier block
 * @data: data
 * @arg: arg
 *
 * This is the IPv4 notifier callback function gets invoked
 * if any change in IP and then invoke the function @__wlan_hdd_ipv4_changed
 * to reconfigure the offload parameters.
 *
 * Return: 0 on success, error number otherwise.
 */
int wlan_hdd_ipv4_changed(struct notifier_block *nb,
				unsigned long data, void *arg)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_ipv4_changed(nb, data, arg);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**----------------------------------------------------------------------------

  \brief hdd_conf_arp_offload() - Configure ARP offload

  Called during SUSPEND to configure the ARP offload (MC BC filter) which
  reduces power consumption.

  \param  - pAdapter -Adapter context for which ARP offload is to be configured
  \param  - fenable - 0 - disable.
                      1 - enable. (with IPv4 notifier registration)
                      2 - enable. (without IPv4 notifier registration)

  \return -
            VOS_STATUS_SUCCESS - on successful operation
            VOS_STATUS_E_FAILURE - on failure of operation
-----------------------------------------------------------------------------*/
VOS_STATUS hdd_conf_arp_offload(hdd_adapter_t *pAdapter, int fenable)
{
   struct in_ifaddr **ifap = NULL;
   struct in_ifaddr *ifa = NULL;
   struct in_device *in_dev;
   int i = 0;
   tSirHostOffloadReq  offLoadRequest;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

   hddLog(LOG1, FL(" fenable = %d \n"), fenable);

   /* In SAP/P2PGo mode, ARP/NS offload feature capability
    * is controlled by one bit.
    */
   if ((WLAN_HDD_SOFTAP == pAdapter->device_mode ||
       WLAN_HDD_P2P_GO == pAdapter->device_mode) &&
       !pHddCtx->ap_arpns_support) {
       hddLog(LOG1, FL("APR Offload is not supported in SAP/P2PGO mode"));
       return VOS_STATUS_SUCCESS;
   }

   if(fenable)
   {
       if ((in_dev = __in_dev_get_rtnl(pAdapter->dev)) != NULL)
       {
           for (ifap = &in_dev->ifa_list; (ifa = *ifap) != NULL;
                   ifap = &ifa->ifa_next)
           {
               if (!strcmp(pAdapter->dev->name, ifa->ifa_label))
               {
                   break; /* found */
               }
           }
       }
       if(ifa && ifa->ifa_local)
       {
           offLoadRequest.offloadType =  SIR_IPV4_ARP_REPLY_OFFLOAD;
           offLoadRequest.enableOrDisable = SIR_OFFLOAD_ENABLE;

           hddLog(VOS_TRACE_LEVEL_INFO, "%s: Enabled", __func__);

           if (((HDD_MCASTBCASTFILTER_FILTER_ALL_BROADCAST ==
                pHddCtx->sus_res_mcastbcast_filter) ||
               (HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST_BROADCAST ==
                pHddCtx->sus_res_mcastbcast_filter)) &&
               (VOS_TRUE == pHddCtx->sus_res_mcastbcast_filter_valid))
           {
               offLoadRequest.enableOrDisable =
                   SIR_OFFLOAD_ARP_AND_BCAST_FILTER_ENABLE;
               hddLog(VOS_TRACE_LEVEL_INFO,
                      "offload: inside arp offload conditional check");
           }

           hddLog(VOS_TRACE_LEVEL_INFO, "offload: arp filter programmed = %d",
                  offLoadRequest.enableOrDisable);

           //converting u32 to IPV4 address
           for(i = 0 ; i < 4; i++)
           {
              offLoadRequest.params.hostIpv4Addr[i] =
                      (ifa->ifa_local >> (i*8) ) & 0xFF ;
           }
           hddLog(VOS_TRACE_LEVEL_INFO, " Enable SME HostOffload: %d.%d.%d.%d",
                  offLoadRequest.params.hostIpv4Addr[0],
                  offLoadRequest.params.hostIpv4Addr[1],
                  offLoadRequest.params.hostIpv4Addr[2],
                  offLoadRequest.params.hostIpv4Addr[3]);

          if (eHAL_STATUS_SUCCESS !=
                    sme_SetHostOffload(WLAN_HDD_GET_HAL_CTX(pAdapter),
                    pAdapter->sessionId, &offLoadRequest))
          {
              hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Failed to enable HostOffload "
                      "feature", __func__);
              return VOS_STATUS_E_FAILURE;
          }
       }
       else
       {
           hddLog(VOS_TRACE_LEVEL_INFO, FL("IP Address is not assigned\n"));
       }

       return VOS_STATUS_SUCCESS;
   }
   else
   {
       vos_mem_zero((void *)&offLoadRequest, sizeof(tSirHostOffloadReq));
       offLoadRequest.enableOrDisable = SIR_OFFLOAD_DISABLE;
       offLoadRequest.offloadType =  SIR_IPV4_ARP_REPLY_OFFLOAD;

       if (eHAL_STATUS_SUCCESS !=
                 sme_SetHostOffload(WLAN_HDD_GET_HAL_CTX(pAdapter),
                 pAdapter->sessionId, &offLoadRequest))
       {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Failure to disable host "
                             "offload feature", __func__);
            return VOS_STATUS_E_FAILURE;
       }
       return VOS_STATUS_SUCCESS;
   }
}

/*
 * This function is called before setting mcbc filters
 * to modify filter value considering Different Offloads
 */

void hdd_mcbc_filter_modification(hdd_context_t* pHddCtx,
                                  tANI_U8 *pMcBcFilter)
{
    if (NULL == pHddCtx)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("NULL HDD context passed"));
        return;
    }

    *pMcBcFilter = pHddCtx->configuredMcastBcastFilter;
    if (pHddCtx->cfg_ini->fhostArpOffload)
    {
        /* ARP offload is enabled, do not block bcast packets at RXP
         * Will be using Bitmasking to reset the filter. As we have
         * disable Broadcast filtering, Anding with the negation
         * of Broadcast BIT
         */
        hddLog(VOS_TRACE_LEVEL_INFO, FL("ARP offload is enabled"));
        *pMcBcFilter &= ~(HDD_MCASTBCASTFILTER_FILTER_ALL_BROADCAST);
    }

#ifdef WLAN_NS_OFFLOAD
    if (pHddCtx->cfg_ini->fhostNSOffload)
    {
        /* NS offload is enabled, do not block mcast packets at RXP
         * Will be using Bitmasking to reset the filter. As we have
         * disable Multicast filtering, Anding with the negation
         * of Multicast BIT
         */
        hddLog(VOS_TRACE_LEVEL_INFO, FL("NS offload is enabled"));
        *pMcBcFilter &= ~(HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST);
    }
#endif

    pHddCtx->configuredMcastBcastFilter = *pMcBcFilter;
}

void hdd_conf_mcastbcast_filter(hdd_context_t* pHddCtx, v_BOOL_t setfilter)
{
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    tpSirWlanSetRxpFilters wlanRxpFilterParam =
                     vos_mem_malloc(sizeof(tSirWlanSetRxpFilters));
    if (NULL == wlanRxpFilterParam)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
           "%s: vos_mem_alloc failed ", __func__);
        return;
    }
    hddLog(VOS_TRACE_LEVEL_INFO,
        "%s: Configuring Mcast/Bcast Filter Setting. setfilter %d", __func__, setfilter);
    if (TRUE == setfilter)
    {
            hdd_mcbc_filter_modification(pHddCtx,
                  &wlanRxpFilterParam->configuredMcstBcstFilterSetting);
    }
    else
    {
        /*Use the current configured value to clear*/
        wlanRxpFilterParam->configuredMcstBcstFilterSetting =
                              pHddCtx->configuredMcastBcastFilter;
    }

    wlanRxpFilterParam->setMcstBcstFilter = setfilter;
    halStatus = sme_ConfigureRxpFilter(pHddCtx->hHal, wlanRxpFilterParam);

    if (setfilter && (eHAL_STATUS_SUCCESS == halStatus))
       pHddCtx->hdd_mcastbcast_filter_set = TRUE;

    hddLog(VOS_TRACE_LEVEL_INFO, "%s to post set/reset filter to"
           "lower mac with status %d"
           "configuredMcstBcstFilterSetting = %d"
           "setMcstBcstFilter = %d",(eHAL_STATUS_SUCCESS != halStatus) ?
           "Failed" : "Success", halStatus,
           wlanRxpFilterParam->configuredMcstBcstFilterSetting,
           wlanRxpFilterParam->setMcstBcstFilter);

    if (eHAL_STATUS_SUCCESS != halStatus)
        vos_mem_free(wlanRxpFilterParam);
}

static void hdd_conf_suspend_ind(hdd_context_t* pHddCtx,
                                 hdd_adapter_t *pAdapter,
                                 void (*callback)(void *callbackContext,
                                                  boolean suspended),
                                 void *callbackContext)
{
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    tpSirWlanSuspendParam wlanSuspendParam =
      vos_mem_malloc(sizeof(tSirWlanSuspendParam));

    if (VOS_FALSE == pHddCtx->sus_res_mcastbcast_filter_valid) {
        pHddCtx->sus_res_mcastbcast_filter =
            pHddCtx->configuredMcastBcastFilter;
        pHddCtx->sus_res_mcastbcast_filter_valid = VOS_TRUE;
        hddLog(VOS_TRACE_LEVEL_INFO, "offload: hdd_conf_suspend_ind");
        hddLog(VOS_TRACE_LEVEL_INFO, "configuredMCastBcastFilter saved = %d",
               pHddCtx->configuredMcastBcastFilter);

    }


    if(NULL == wlanSuspendParam)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
           "%s: vos_mem_alloc failed ", __func__);
        return;
    }

    hddLog(VOS_TRACE_LEVEL_INFO,
      "%s: send wlan suspend indication", __func__);

    if((pHddCtx->cfg_ini->nEnableSuspend == WLAN_MAP_SUSPEND_TO_MCAST_BCAST_FILTER))
    {
        //Configure supported OffLoads
        hdd_conf_hostoffload(pAdapter, TRUE);
        wlanSuspendParam->configuredMcstBcstFilterSetting = pHddCtx->configuredMcastBcastFilter;
    }

    if ((eConnectionState_Associated ==
            (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState) ||
        (eConnectionState_IbssConnected ==
            (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState))
                wlanSuspendParam->connectedState = TRUE;
    else
                wlanSuspendParam->connectedState = FALSE;

    wlanSuspendParam->sessionId = pAdapter->sessionId;
    halStatus = sme_ConfigureSuspendInd(pHddCtx->hHal, wlanSuspendParam,
                                        callback, callbackContext);
    if(eHAL_STATUS_SUCCESS == halStatus)
    {
        pHddCtx->hdd_mcastbcast_filter_set = TRUE;
    } else {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("sme_ConfigureSuspendInd returned failure %d"), halStatus);

        vos_mem_free(wlanSuspendParam);
    }
}

static void hdd_conf_resume_ind(hdd_adapter_t *pAdapter)
{
    hdd_context_t* pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    eHalStatus halStatus = eHAL_STATUS_FAILURE;


    halStatus = sme_ConfigureResumeReq(pHddCtx->hHal,
                                       NULL
                                      );

    if (eHAL_STATUS_SUCCESS != halStatus)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: sme_ConfigureResumeReq return failure %d",
               __func__, halStatus);

    }

    hddLog(VOS_TRACE_LEVEL_INFO,
      "%s: send wlan resume indication", __func__);
    /* Disable supported OffLoads */
    hdd_conf_hostoffload(pAdapter, FALSE);
    pHddCtx->hdd_mcastbcast_filter_set = FALSE;

    if (VOS_TRUE == pHddCtx->sus_res_mcastbcast_filter_valid) {
        pHddCtx->configuredMcastBcastFilter =
            pHddCtx->sus_res_mcastbcast_filter;
        pHddCtx->sus_res_mcastbcast_filter_valid = VOS_FALSE;
    }

    hddLog(VOS_TRACE_LEVEL_INFO,
           "offload: in hdd_conf_resume_ind, restoring configuredMcastBcastFilter");
    hddLog(VOS_TRACE_LEVEL_INFO, "configuredMcastBcastFilter = %d",
                  pHddCtx->configuredMcastBcastFilter);
}

//Suspend routine registered with Android OS
void hdd_suspend_wlan(void (*callback)(void *callbackContext, boolean suspended),
                      void *callbackContext)
{
   hdd_context_t *pHddCtx = NULL;
   v_CONTEXT_t pVosContext = NULL;

   VOS_STATUS status;
   hdd_adapter_t *pAdapter = NULL;
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   bool hdd_enter_bmps = FALSE;

   hddLog(VOS_TRACE_LEVEL_INFO, "%s: WLAN being suspended by Android OS",__func__);

   //Get the global VOSS context.
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   if(!pVosContext) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Global VOS context is Null", __func__);
      return;
   }

   //Get the HDD context.
   pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext );

   if(!pHddCtx) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD context is Null",__func__);
      return;
   }

   if (pHddCtx->isLogpInProgress) {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Ignore suspend wlan, LOGP in progress!", __func__);
      return;
   }

   hdd_set_pwrparams(pHddCtx);
   status =  hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
       pAdapter = pAdapterNode->pAdapter;
       if ( (WLAN_HDD_INFRA_STATION != pAdapter->device_mode)
         && (WLAN_HDD_SOFTAP != pAdapter->device_mode)
         && (WLAN_HDD_P2P_CLIENT != pAdapter->device_mode) )

       {
           goto send_suspend_ind;
       }
       /* Avoid multiple enter/exit BMPS in this while loop using
        * hdd_enter_bmps flag
        */
       if (FALSE == hdd_enter_bmps && (BMPS == pmcGetPmcState(pHddCtx->hHal)))
       {
            hdd_enter_bmps = TRUE;

           /* If device was already in BMPS, and dynamic DTIM is set,
            * exit(set the device to full power) and enter BMPS again
            * to reflect new DTIM value */
           wlan_hdd_enter_bmps(pAdapter, DRIVER_POWER_MODE_ACTIVE);

           wlan_hdd_enter_bmps(pAdapter, DRIVER_POWER_MODE_AUTO);

           pHddCtx->hdd_ignore_dtim_enabled = TRUE;
       }
#ifdef SUPPORT_EARLY_SUSPEND_STANDBY_DEEPSLEEP
       if (pHddCtx->cfg_ini->nEnableSuspend == WLAN_MAP_SUSPEND_TO_STANDBY)
       {
          //stop the interface before putting the chip to standby
          hddLog(LOG1, FL("Disabling queues"));
          netif_tx_disable(pAdapter->dev);
          netif_carrier_off(pAdapter->dev);
       }
       else if (pHddCtx->cfg_ini->nEnableSuspend ==
               WLAN_MAP_SUSPEND_TO_DEEP_SLEEP)
       {
          //Execute deep sleep procedure
          hdd_enter_deep_sleep(pHddCtx, pAdapter);
       }
#endif

send_suspend_ind:
       //stop all TX queues before suspend
       hddLog(LOG1, FL("Disabling queues"));
       netif_tx_disable(pAdapter->dev);
       WLANTL_PauseUnPauseQs(pVosContext, true);

      /* Keep this suspend indication at the end (before processing next adaptor)
       * for discrete. This indication is considered as trigger point to start
       * WOW (if wow is enabled). */
       /*Suspend notification sent down to driver*/
       hdd_conf_suspend_ind(pHddCtx, pAdapter, callback, callbackContext);

       status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
       pAdapterNode = pNext;
   }

   pHddCtx->hdd_wlan_suspended = TRUE;

#ifdef SUPPORT_EARLY_SUSPEND_STANDBY_DEEPSLEEP
  if(pHddCtx->cfg_ini->nEnableSuspend == WLAN_MAP_SUSPEND_TO_STANDBY)
  {
      hdd_enter_standby(pHddCtx);
  }
#endif

   return;
}

static void hdd_PowerStateChangedCB
(
   v_PVOID_t callbackContext,
   tPmcState newState
)
{
   hdd_context_t *pHddCtx = callbackContext;
   /* if the driver was not in BMPS during early suspend,
    * the dynamic DTIM is now updated at Riva */
   if ((newState == BMPS) && pHddCtx->hdd_wlan_suspended
           && pHddCtx->cfg_ini->enableDynamicDTIM
           && (pHddCtx->hdd_ignore_dtim_enabled == FALSE))
   {
       pHddCtx->hdd_ignore_dtim_enabled = TRUE;
   }
   spin_lock(&pHddCtx->filter_lock);
   if ((newState == BMPS) &&  pHddCtx->hdd_wlan_suspended)
   {
      spin_unlock(&pHddCtx->filter_lock);
      if (VOS_FALSE == pHddCtx->sus_res_mcastbcast_filter_valid)
      {
          pHddCtx->sus_res_mcastbcast_filter =
              pHddCtx->configuredMcastBcastFilter;
          pHddCtx->sus_res_mcastbcast_filter_valid = VOS_TRUE;

          hddLog(VOS_TRACE_LEVEL_INFO, "offload: callback to associated");
          hddLog(VOS_TRACE_LEVEL_INFO, "saving configuredMcastBcastFilter = %d",
                 pHddCtx->configuredMcastBcastFilter);
          hddLog(VOS_TRACE_LEVEL_INFO,
                 "offload: calling hdd_conf_mcastbcast_filter");

      }

      hdd_conf_mcastbcast_filter(pHddCtx, TRUE);
      if(pHddCtx->hdd_mcastbcast_filter_set != TRUE)
         hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Not able to set mcast/bcast filter ", __func__);
   }
   else
   {
      /* Android framework can send resume request when the WCN chip is
       * in IMPS mode. When the chip exits IMPS mode the firmware will
       * restore all the registers to the state they were before the chip
       * entered IMPS and so our hardware filter settings configured by the
       * resume request will be lost. So reconfigure the filters on detecting
       * a change in the power state of the WCN chip.
       */
      spin_unlock(&pHddCtx->filter_lock);
      if (IMPS != newState)
      {
           spin_lock(&pHddCtx->filter_lock);
           if (FALSE == pHddCtx->hdd_wlan_suspended)
           {
                spin_unlock(&pHddCtx->filter_lock);
                hddLog(VOS_TRACE_LEVEL_INFO,
                          "Not in IMPS/BMPS and suspended state");
                hdd_conf_mcastbcast_filter(pHddCtx, FALSE);
           }
           else
           {
                spin_unlock(&pHddCtx->filter_lock);
           }
      }
   }
}



void hdd_register_mcast_bcast_filter(hdd_context_t *pHddCtx)
{
   v_CONTEXT_t pVosContext;
   tHalHandle smeContext;

   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   if (NULL == pVosContext)
   {
      hddLog(LOGE, "%s: Invalid pContext", __func__);
      return;
   }
   smeContext = vos_get_context(VOS_MODULE_ID_SME, pVosContext);
   if (NULL == smeContext)
   {
      hddLog(LOGE, "%s: Invalid smeContext", __func__);
      return;
   }

   spin_lock_init(&pHddCtx->filter_lock);
   if (WLAN_MAP_SUSPEND_TO_MCAST_BCAST_FILTER ==
                                            pHddCtx->cfg_ini->nEnableSuspend)
   {
      if(!pHddCtx->cfg_ini->enablePowersaveOffload)
      {
         pmcRegisterDeviceStateUpdateInd(smeContext,
                   hdd_PowerStateChangedCB, pHddCtx);
      }
      /* TODO: For Power Save Offload case */
   }
}

void hdd_unregister_mcast_bcast_filter(hdd_context_t *pHddCtx)
{
   v_CONTEXT_t pVosContext;
   tHalHandle smeContext;

   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   if (NULL == pVosContext)
   {
      hddLog(LOGE, "%s: Invalid pContext", __func__);
      return;
   }
   smeContext = vos_get_context(VOS_MODULE_ID_SME, pVosContext);
   if (NULL == smeContext)
   {
      hddLog(LOGE, "%s: Invalid smeContext", __func__);
      return;
   }

   if (WLAN_MAP_SUSPEND_TO_MCAST_BCAST_FILTER ==
                                            pHddCtx->cfg_ini->nEnableSuspend)
   {
      if(!pHddCtx->cfg_ini->enablePowersaveOffload)
      {
         pmcDeregisterDeviceStateUpdateInd(smeContext,
                             hdd_PowerStateChangedCB);
      }
      /* TODO: For Power Save Offload case */
   }
}

void hdd_resume_wlan(void)
{
   hdd_context_t *pHddCtx = NULL;
   hdd_adapter_t *pAdapter = NULL;
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   v_CONTEXT_t pVosContext = NULL;

   hddLog(VOS_TRACE_LEVEL_INFO, "%s: WLAN being resumed by Android OS",__func__);

   //Get the global VOSS context.
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   if(!pVosContext) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Global VOS context is Null", __func__);
      return;
   }

   //Get the HDD context.
   pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext );

   if(!pHddCtx) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD context is Null",__func__);
      return;
   }

   if (pHddCtx->isLogpInProgress)
   {
      hddLog(VOS_TRACE_LEVEL_INFO,
             "%s: Ignore resume wlan, LOGP in progress!", __func__);
      return;
   }

   pHddCtx->hdd_wlan_suspended = FALSE;

   /*loop through all adapters. Concurrency */
   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
       pAdapter = pAdapterNode->pAdapter;
       if ( (WLAN_HDD_INFRA_STATION != pAdapter->device_mode)
         && (WLAN_HDD_SOFTAP != pAdapter->device_mode)
         && (WLAN_HDD_P2P_CLIENT != pAdapter->device_mode) )
       {
            goto send_resume_ind;
       }


#ifdef SUPPORT_EARLY_SUSPEND_STANDBY_DEEPSLEEP
       if(pHddCtx->hdd_ps_state == eHDD_SUSPEND_DEEP_SLEEP)
       {
          hddLog(VOS_TRACE_LEVEL_INFO, "%s: WLAN being resumed from deep sleep",__func__);
          hdd_exit_deep_sleep(pAdapter);
       }
#endif

      if(pHddCtx->hdd_ignore_dtim_enabled == TRUE)
      {
         /*Switch back to DTIM 1*/
         tSirSetPowerParamsReq powerRequest = { 0 };

         powerRequest.uIgnoreDTIM = pHddCtx->hdd_actual_ignore_DTIM_value;
         powerRequest.uListenInterval = pHddCtx->hdd_actual_LI_value;
         powerRequest.uMaxLIModulatedDTIM = pHddCtx->cfg_ini->fMaxLIModulatedDTIM;

         /*Disabled ModulatedDTIM if enabled on suspend*/
         if(pHddCtx->cfg_ini->enableModulatedDTIM)
             powerRequest.uDTIMPeriod = 0;

         /* Update ignoreDTIM and ListedInterval in CFG with default values */
         ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_IGNORE_DTIM, powerRequest.uIgnoreDTIM,
                          NULL, eANI_BOOLEAN_FALSE);
         ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_LISTEN_INTERVAL, powerRequest.uListenInterval,
                          NULL, eANI_BOOLEAN_FALSE);

         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                        "Switch to DTIM%d",powerRequest.uListenInterval);
         sme_SetPowerParams( WLAN_HDD_GET_HAL_CTX(pAdapter), &powerRequest, FALSE);

         if (BMPS == pmcGetPmcState(pHddCtx->hHal))
         {
             /* put the device into full power */
             wlan_hdd_enter_bmps(pAdapter, DRIVER_POWER_MODE_ACTIVE);

             /* put the device back into BMPS */
             wlan_hdd_enter_bmps(pAdapter, DRIVER_POWER_MODE_AUTO);

             pHddCtx->hdd_ignore_dtim_enabled = FALSE;
         }
      }

send_resume_ind:
      //wake the tx queues
      hddLog(LOG1, FL("Enabling queues"));
      WLANTL_PauseUnPauseQs(pVosContext, false);

      netif_tx_wake_all_queues(pAdapter->dev);

      hdd_conf_resume_ind(pAdapter);

      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

#ifdef IPA_OFFLOAD
   hdd_ipa_resume(pHddCtx);
#endif

#ifdef SUPPORT_EARLY_SUSPEND_STANDBY_DEEPSLEEP
   if(pHddCtx->hdd_ps_state == eHDD_SUSPEND_STANDBY)
   {
       hdd_exit_standby(pHddCtx);
   }
#endif

   return;
}

VOS_STATUS hdd_wlan_reset_initialization(void)
{
   v_CONTEXT_t pVosContext = NULL;

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: WLAN being reset",__func__);

   //Get the global VOSS context.
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   if(!pVosContext)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Global VOS context is Null", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Preventing the phone from going to suspend",__func__);

   // Prevent the phone from going to sleep
   hdd_prevent_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_REINIT);

   return VOS_STATUS_SUCCESS;
}

static void hdd_ssr_timer_init(void)
{
    init_timer(&ssr_timer);
}

static void hdd_ssr_timer_del(void)
{
    del_timer(&ssr_timer);
    ssr_timer_started = false;
}

static void hdd_ssr_timer_cb(unsigned long data)
{
    hddLog(VOS_TRACE_LEVEL_FATAL, "%s: HDD SSR timer expired!", __func__);
    VOS_BUG(0);
}

static void hdd_ssr_timer_start(int msec)
{
    if(ssr_timer_started)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Trying to start SSR timer when "
               "it's running!", __func__);
    }
    ssr_timer.expires = jiffies + msecs_to_jiffies(msec);
    ssr_timer.function = hdd_ssr_timer_cb;
    add_timer(&ssr_timer);
    ssr_timer_started = true;
}

/* the HDD interface to WLAN driver shutdown,
 * the primary shutdown function in SSR
 */
VOS_STATUS hdd_wlan_shutdown(void)
{
   VOS_STATUS       vosStatus;
   v_CONTEXT_t      pVosContext = NULL;
   hdd_context_t    *pHddCtx = NULL;
   pVosSchedContext vosSchedContext = NULL;

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: WLAN driver shutting down! ",__func__);

#ifdef WLAN_FEATURE_LPSS
   wlan_hdd_send_status_pkg(NULL, NULL, 0, 0);
#endif

   /* If SSR never completes, then do kernel panic. */
   hdd_ssr_timer_init();
   hdd_ssr_timer_start(HDD_SSR_BRING_UP_TIME);

   /* Get the global VOSS context. */
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   if(!pVosContext) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Global VOS context is Null", __func__);
      return VOS_STATUS_E_FAILURE;
   }
   /* Get the HDD context. */
   pHddCtx = (hdd_context_t*)vos_get_context(VOS_MODULE_ID_HDD, pVosContext);
   if(!pHddCtx) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD context is Null",__func__);
      return VOS_STATUS_E_FAILURE;
   }

   pHddCtx->isLogpInProgress = TRUE;
   vos_set_logp_in_progress(VOS_MODULE_ID_VOSS, TRUE);

   vos_clear_concurrent_session_count();

#ifdef FEATURE_BUS_BANDWIDTH
   if (VOS_TIMER_STATE_RUNNING ==
           vos_timer_getCurrentState(&pHddCtx->bus_bw_timer))
   {
      vos_timer_stop(&pHddCtx->bus_bw_timer);
   }
#endif

   hdd_reset_all_adapters(pHddCtx);

#ifdef IPA_UC_OFFLOAD
   hdd_ipa_uc_ssr_deinit();
#endif

   vosStatus = hddDevTmUnregisterNotifyCallback(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hddDevTmUnregisterNotifyCallback failed",__func__);
   }

   /* Disable IMPS/BMPS as we do not want the device to enter any power
    * save mode on its own during reset sequence
    */
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_UAPSD_MODE_POWER_SAVE);

   vosSchedContext = get_vos_sched_ctxt();

   /* Wakeup all driver threads */
   if(TRUE == pHddCtx->isMcThreadSuspended){
      complete(&vosSchedContext->ResumeMcEvent);
      pHddCtx->isMcThreadSuspended= FALSE;
   }
#ifdef QCA_CONFIG_SMP
   if (TRUE == pHddCtx->isTlshimRxThreadSuspended) {
      complete(&vosSchedContext->ResumeTlshimRxEvent);
      pHddCtx->isTlshimRxThreadSuspended = FALSE;
    }
#endif

   /* Reset the Suspend Variable */
   pHddCtx->isWlanSuspended = FALSE;

   /* Stop all the threads; we do not want any messages to be a processed,
    * any more and the best way to ensure that is to terminate the threads
    * gracefully.
    */
   /* Wait for MC to exit */
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Shutting down MC thread",__func__);
   set_bit(MC_SHUTDOWN_EVENT_MASK, &vosSchedContext->mcEventFlag);
   set_bit(MC_POST_EVENT_MASK, &vosSchedContext->mcEventFlag);
   wake_up_interruptible(&vosSchedContext->mcWaitQueue);
   wait_for_completion(&vosSchedContext->McShutdown);

#ifdef QCA_CONFIG_SMP
   /* Wait for TLshim RX to exit */
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Shutting down TLshim RX thread",
          __func__);
   unregister_hotcpu_notifier(vosSchedContext->cpuHotPlugNotifier);
   set_bit(RX_SHUTDOWN_EVENT_MASK, &vosSchedContext->tlshimRxEvtFlg);
   set_bit(RX_POST_EVENT_MASK, &vosSchedContext->tlshimRxEvtFlg);
   wake_up_interruptible(&vosSchedContext->tlshimRxWaitQueue);
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Waiting for TLshim RX thread to exit",
          __func__);
   wait_for_completion(&vosSchedContext->TlshimRxShutdown);
   vosSchedContext->TlshimRxThread = NULL;
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Waiting for dropping RX packets",
          __func__);
   vos_drop_rxpkt_by_staid(vosSchedContext, WLAN_MAX_STA_COUNT);
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Waiting for freeing freeQ", __func__);
   vos_free_tlshim_pkt_freeq(vosSchedContext);
#endif


   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing WDA STOP", __func__);
   vosStatus = WDA_stop(pVosContext, HAL_STOP_TYPE_RF_KILL);

   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to stop WDA", __func__);
      VOS_ASSERT(VOS_IS_STATUS_SUCCESS(vosStatus));
      WDA_setNeedShutdown(pVosContext);
   }

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing SME STOP",__func__);
   /* Stop SME - Cannot invoke vos_stop as vos_stop relies
    * on threads being running to process the SYS Stop
    */
   vosStatus = sme_Stop(pHddCtx->hHal, HAL_STOP_TYPE_SYS_RESET);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to stop sme %d", __func__, vosStatus);
       VOS_ASSERT(0);
   }

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing MAC STOP",__func__);
   /* Stop MAC (PE and HAL) */
   vosStatus = macStop(pHddCtx->hHal, HAL_STOP_TYPE_SYS_RESET);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to stop mac %d", __func__, vosStatus);
       VOS_ASSERT(0);
   }

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing TL STOP",__func__);
   /* Stop TL */
   vosStatus = WLANTL_Stop(pVosContext);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to stop TL %d", __func__, vosStatus);
       VOS_ASSERT(0);
   }

   hdd_unregister_mcast_bcast_filter(pHddCtx);

   hddLog(VOS_TRACE_LEVEL_INFO, "%s: Flush Queues",__func__);
   /* Clean up message queues of TX, RX and MC thread */
   vos_sched_flush_mc_mqs(vosSchedContext);

   /* Deinit all the TX, RX and MC queues */
   vos_sched_deinit_mqs(vosSchedContext);

   hddLog(VOS_TRACE_LEVEL_INFO, "%s: Doing VOS Shutdown",__func__);
   /* shutdown VOSS */
   vos_shutdown(pVosContext);

   /*mac context has already been released in mac_close call
     so setting it to NULL in hdd context*/
   pHddCtx->hHal = (tHalHandle)NULL;

   if (free_riva_power_on_lock("wlan"))
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to free power on lock",
                                           __func__);
   }
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: WLAN driver shutdown complete"
                                   ,__func__);
   return VOS_STATUS_SUCCESS;
}



/* the HDD interface to WLAN driver re-init.
 * This is called to initialize/start WLAN driver after a shutdown.
 */
VOS_STATUS hdd_wlan_re_init(void *hif_sc)
{
   VOS_STATUS       vosStatus;
   v_CONTEXT_t      pVosContext = NULL;
   hdd_context_t    *pHddCtx = NULL;
   eHalStatus       halStatus;

   hdd_adapter_t *pAdapter;
   int i;
   hdd_prevent_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_REINIT);

   vos_set_reinit_in_progress(VOS_MODULE_ID_VOSS, TRUE);

   /* Get the VOS context */
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   if(pVosContext == NULL)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Failed vos_get_global_context",
             __func__);
      goto err_re_init;
   }

   /* Get the HDD context */
   pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext);
   if(!pHddCtx)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: HDD context is Null", __func__);
      goto err_re_init;
   }

   if (!hif_sc) {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: hif_sc is NULL", __func__);
      goto err_re_init;
   }

   ((VosContextType*)pVosContext)->pHIFContext = hif_sc;

   /* The driver should always be initialized in STA mode after SSR */
   hdd_set_conparam(0);

   /* Re-open VOSS, it is a re-open b'se control transport was never closed. */
   vosStatus = vos_open(&pVosContext, 0);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_open failed",__func__);
      goto err_re_init;
   }

#if  !defined(REMOVE_PKT_LOG)
      hif_init_pdev_txrx_handle(hif_sc,
      vos_get_context(VOS_MODULE_ID_TXRX, pVosContext));
#endif

   /* Save the hal context in Adapter */
   pHddCtx->hHal = (tHalHandle)vos_get_context( VOS_MODULE_ID_SME, pVosContext );
   if ( NULL == pHddCtx->hHal )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HAL context is null",__func__);
      goto err_vosclose;
   }

   /* Set the SME configuration parameters. */
   vosStatus = hdd_set_sme_config(pHddCtx);
   if ( VOS_STATUS_SUCCESS != vosStatus )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Failed hdd_set_sme_config",__func__);
      goto err_vosclose;
   }

   vosStatus = vos_preStart( pHddCtx->pvosContext );
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_preStart failed",__func__);
      goto err_vosclose;
   }

   vosStatus = hdd_set_sme_chan_list(pHddCtx);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus)) {
      hddLog(VOS_TRACE_LEVEL_FATAL,
             "%s: Failed to init channel list", __func__);
      goto err_vosclose;
   }

   /* In the integrated architecture we update the configuration from
      the INI file and from NV before vOSS has been started so that
      the final contents are available to send down to the cCPU   */
   /* Apply the cfg.ini to cfg.dat */
   if (FALSE == hdd_update_config_dat(pHddCtx))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: config update failed",__func__ );
      goto err_vosclose;
   }

   /* Set the MAC Address, currently this is used by HAL to add self sta.
    * Remove this once self sta is added as part of session open. */
   halStatus = cfgSetStr(pHddCtx->hHal, WNI_CFG_STA_ID,
         (v_U8_t *)&pHddCtx->cfg_ini->intfMacAddr[0],
           sizeof(pHddCtx->cfg_ini->intfMacAddr[0]));
   if (!HAL_STATUS_SUCCESS(halStatus))
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Failed to set MAC Address. "
            "HALStatus is %08d [x%08x]",__func__, halStatus, halStatus);
      goto err_vosclose;
   }

   /* Start VOSS which starts up the SME/MAC/HAL modules and everything else
      Note: Firmware image will be read and downloaded inside vos_start API */
   vosStatus = vos_start( pVosContext );
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_start failed",__func__);
      goto err_vosclose;
   }
#ifdef IPA_UC_OFFLOAD
   if (hdd_ipa_uc_ssr_reinit())
      hddLog(LOGE, "%s: HDD IPA UC reinit failed", __func__);
#endif


   vosStatus = hdd_post_voss_start_config( pHddCtx );
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hdd_post_voss_start_config failed",
         __func__);
      goto err_vosstop;
   }

   /* Try to get an adapter from mode ID */
   pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_INFRA_STATION);
   if (!pAdapter) {
      pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_SOFTAP);
      if (!pAdapter) {
        pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_IBSS);
        if (!pAdapter) {
           hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Failed to get Adapter!",
                  __func__);
        }
     }
   }

   /* Get WLAN Host/FW/HW version */
   if (pAdapter)
      hdd_wlan_get_version(pAdapter, NULL, NULL);

   /* Pass FW version to HIF layer */
   hif_set_fw_info(hif_sc, pHddCtx->target_fw_version);

   /* Restart all adapters */
   hdd_start_all_adapters(pHddCtx);

   /* Reconfigure FW logs after SSR */
   if (pAdapter) {
      if (pHddCtx->fw_log_settings.enable != 0) {
         process_wma_set_command(pAdapter->sessionId,
                                 WMI_DBGLOG_MODULE_ENABLE,
                                 pHddCtx->fw_log_settings.enable , DBG_CMD);
      } else {
         process_wma_set_command(pAdapter->sessionId,
                                 WMI_DBGLOG_MODULE_DISABLE,
                                 pHddCtx->fw_log_settings.enable, DBG_CMD);
      }

      if (pHddCtx->fw_log_settings.dl_report != 0) {
         process_wma_set_command(pAdapter->sessionId,
                                 WMI_DBGLOG_REPORT_ENABLE,
                                 pHddCtx->fw_log_settings.dl_report, DBG_CMD);

         process_wma_set_command(pAdapter->sessionId,
                                 WMI_DBGLOG_TYPE,
                                 pHddCtx->fw_log_settings.dl_type, DBG_CMD);

         process_wma_set_command(pAdapter->sessionId,
                                 WMI_DBGLOG_LOG_LEVEL,
                                 pHddCtx->fw_log_settings.dl_loglevel, DBG_CMD);

         for (i = 0; i < MAX_MOD_LOGLEVEL; i++) {
            if (pHddCtx->fw_log_settings.dl_mod_loglevel[i] != 0) {
               process_wma_set_command(pAdapter->sessionId,
                                 WMI_DBGLOG_MOD_LOG_LEVEL,
                                 pHddCtx->fw_log_settings.dl_mod_loglevel[i],
                                 DBG_CMD);
            }
         }
      }
   }

   /* Register TM level change handler function to the platform */
   hddDevTmRegisterNotifyCallback(pHddCtx);

   pHddCtx->hdd_mcastbcast_filter_set = FALSE;
   pHddCtx->btCoexModeSet = false;
   hdd_register_mcast_bcast_filter(pHddCtx);
   hdd_ssr_timer_del();

   wlan_hdd_send_svc_nlink_msg(WLAN_SVC_FW_CRASHED_IND, NULL, 0);

   /* Allow the phone to go to sleep */
   hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_REINIT);
   /* register for riva power on lock */
   if (req_riva_power_on_lock("wlan"))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: req riva power on lock failed",
                                        __func__);
      goto err_unregister_pmops;
   }
   vos_set_reinit_in_progress(VOS_MODULE_ID_VOSS, FALSE);
#ifdef FEATURE_WLAN_EXTSCAN
   sme_ExtScanRegisterCallback(pHddCtx->hHal,
                               wlan_hdd_cfg80211_extscan_callback);
#endif /* FEATURE_WLAN_EXTSCAN */
   sme_set_rssi_threshold_breached_cb(pHddCtx->hHal, hdd_rssi_threshold_breached);

#ifdef WLAN_FEATURE_LPSS
   wlan_hdd_send_all_scan_intf_info(pHddCtx);
   wlan_hdd_send_version_pkg(pHddCtx->target_fw_version,
                             pHddCtx->target_hw_version,
                             pHddCtx->target_hw_name);
#endif
   ol_pktlog_init(hif_sc);
   goto success;

err_unregister_pmops:
#ifdef CONFIG_HAS_EARLYSUSPEND
   hdd_unregister_mcast_bcast_filter(pHddCtx);
#endif
   hdd_close_all_adapters(pHddCtx);

err_vosstop:
   vos_stop(pVosContext);

err_vosclose:
   vos_close(pVosContext);
   vos_sched_close(pVosContext);
   if (pHddCtx)
   {
       /* Unregister the Net Device Notifier */
       unregister_netdevice_notifier(&hdd_netdev_notifier);
       /* Clean up HDD Nlink Service */
       send_btc_nlink_msg(WLAN_MODULE_DOWN_IND, 0);
#ifdef WLAN_KD_READY_NOTIFIER
       cnss_diag_notify_wlan_close();
       nl_srv_exit(pHddCtx->ptt_pid);
#else
       nl_srv_exit();
#endif /* WLAN_KD_READY_NOTIFIER */
       /* Free up dynamically allocated members inside HDD Adapter */
       kfree(pHddCtx->cfg_ini);
       pHddCtx->cfg_ini= NULL;
       wlan_hdd_deinit_tx_rx_histogram(pHddCtx);
       wiphy_unregister(pHddCtx->wiphy);
       wiphy_free(pHddCtx->wiphy);
   }
   vos_preClose(&pVosContext);

#ifdef MEMORY_DEBUG
   vos_mem_exit();
#endif

err_re_init:
   /* Allow the phone to go to sleep */
   hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_REINIT);
   vos_set_reinit_in_progress(VOS_MODULE_ID_VOSS, FALSE);
   VOS_BUG(0);
   return -EPERM;

success:
   /* Trigger replay of BTC events */
   send_btc_nlink_msg(WLAN_MODULE_DOWN_IND, 0);
   pHddCtx->isLogpInProgress = FALSE;
   return VOS_STATUS_SUCCESS;
}
