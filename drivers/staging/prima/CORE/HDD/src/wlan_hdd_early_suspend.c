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

/**=============================================================================
*     wlan_hdd_early_suspend.c
*
*     \brief      power management functions
*
*     Description
*                 Copyright 2009 (c) Qualcomm, Incorporated.
*                 All Rights Reserved.
*                 Qualcomm Confidential and Proprietary.
*
==============================================================================**/
/* $HEADER$ */

/**-----------------------------------------------------------------------------
*   Include files
* ----------------------------------------------------------------------------*/

#include <linux/pm.h>
#include <linux/wait.h>
#include <wlan_hdd_includes.h>
#include <wlan_qct_driver.h>
#include <linux/wakelock.h>

#include "halTypes.h"
#include "sme_Api.h"
#include <vos_api.h>
#include "vos_power.h"
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

#include <linux/semaphore.h>
#include <wlan_hdd_hostapd.h>
#include "cfgApi.h"

#ifdef WLAN_BTAMP_FEATURE
#include "bapApi.h"
#include "bap_hdd_main.h"
#include "bap_hdd_misc.h"
#endif

#include <linux/wcnss_wlan.h>
#include <linux/inetdevice.h>
#include <wlan_hdd_cfg.h>
#include <wlan_hdd_cfg80211.h>
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

#define HDD_SSR_BRING_UP_TIME 10000

static eHalStatus g_full_pwr_status;
static eHalStatus g_standby_status;

extern VOS_STATUS hdd_post_voss_start_config(hdd_context_t* pHddCtx);
extern VOS_STATUS vos_chipExitDeepSleepVREGHandler(
   vos_call_status_type* status,
   vos_power_cb_type callback,
   v_PVOID_t user_data);
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

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: WLAN being resumed from standby",__func__);
    INIT_COMPLETION(pHddCtx->full_pwr_comp_var);

   g_full_pwr_status = eHAL_STATUS_FAILURE;
    status = sme_RequestFullPower(pHddCtx->hHal, hdd_suspend_full_pwr_callback, pHddCtx,
      eSME_FULL_PWR_NEEDED_BY_HDD);

   if(status == eHAL_STATUS_PMC_PENDING)
   {
      //Block on a completion variable. Can't wait forever though
      wait_for_completion_interruptible_timeout(&pHddCtx->full_pwr_comp_var, 
         msecs_to_jiffies(WLAN_WAIT_TIME_FULL_PWR));
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
      wait_for_completion_interruptible_timeout(&pHddCtx->full_pwr_comp_var, 
         msecs_to_jiffies(WLAN_WAIT_TIME_FULL_PWR));
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
      wait_for_completion_timeout(&pHddCtx->standby_comp_var, 
         msecs_to_jiffies(WLAN_WAIT_TIME_STANDBY));
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
   vos_call_status_type callType;

   //Stop the Interface TX queue.
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
      wait_for_completion_interruptible_timeout(&pHddCtx->full_pwr_comp_var, 
         msecs_to_jiffies(WLAN_WAIT_TIME_FULL_PWR));
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
      wait_for_completion_interruptible_timeout(&pAdapter->disconnect_comp_var, 
         msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
   }


   //None of the steps should fail after this. Continue even in case of failure
   vosStatus = vos_stop( pHddCtx->pvosContext );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );

   vosStatus = vos_chipAssertDeepSleep( &callType, NULL, NULL );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );

   //Vote off any PMIC voltage supplies
   vosStatus = vos_chipPowerDown(NULL, NULL, NULL);

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

   //Power Up Libra WLAN card first if not already powered up
   vosStatus = vos_chipPowerUp(NULL,NULL,NULL);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Libra WLAN not Powered Up. "
          "exiting", __func__);
      goto err_deep_sleep;
   }

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


   //Open a SME session for future operation
   halStatus = sme_OpenSession( pHddCtx->hHal, hdd_smeRoamCallback, pHddCtx,
                                (tANI_U8 *)&pAdapter->macAddressCurrent, &pAdapter->sessionId );
   if ( !HAL_STATUS_SUCCESS( halStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"sme_OpenSession() failed with status code %08d [x%08lx]",
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

VOS_STATUS hdd_conf_hostarpoffload(hdd_adapter_t *pAdapter, v_BOOL_t fenable)
{
   struct in_ifaddr **ifap = NULL;
   struct in_ifaddr *ifa = NULL;
   struct in_device *in_dev;
   int i = 0;
   tSirHostOffloadReq  offLoadRequest;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

   hddLog(VOS_TRACE_LEVEL_ERROR, "%s: \n", __func__);

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

           hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Enabled \n", __func__);

           if(pHddCtx->dynamic_mcbc_filter.enableCfg)
           {
               if((HDD_MCASTBCASTFILTER_FILTER_ALL_BROADCAST == 
              pHddCtx->dynamic_mcbc_filter.mcastBcastFilterSetting) ||
              (HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST_BROADCAST == 
              pHddCtx->dynamic_mcbc_filter.mcastBcastFilterSetting))
               {
                   offLoadRequest.enableOrDisable = 
                           SIR_OFFLOAD_ARP_AND_BCAST_FILTER_ENABLE;
               }
           }
           else if((HDD_MCASTBCASTFILTER_FILTER_ALL_BROADCAST ==
              pHddCtx->cfg_ini->mcastBcastFilterSetting ) || 
              (HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST_BROADCAST ==
              pHddCtx->cfg_ini->mcastBcastFilterSetting))
           {
               offLoadRequest.enableOrDisable = 
                       SIR_OFFLOAD_ARP_AND_BCAST_FILTER_ENABLE;
           }
           
           //converting u32 to IPV4 address
           for(i = 0 ; i < 4; i++)
           {
              offLoadRequest.params.hostIpv4Addr[i] = 
                      (ifa->ifa_local >> (i*8) ) & 0xFF ;
           }
           hddLog(VOS_TRACE_LEVEL_WARN, " Enable SME HostOffload: %d.%d.%d.%d",
                  offLoadRequest.params.hostIpv4Addr[0],
                  offLoadRequest.params.hostIpv4Addr[1],
                  offLoadRequest.params.hostIpv4Addr[2],
                  offLoadRequest.params.hostIpv4Addr[3]);

          if (eHAL_STATUS_SUCCESS != 
                    sme_SetHostOffload(WLAN_HDD_GET_HAL_CTX(pAdapter), 
                                       pAdapter->sessionId, &offLoadRequest))
          {
              hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Failed to enable HostOffload "
                      "feature\n", __func__);
              return VOS_STATUS_E_FAILURE;
          }
          return VOS_STATUS_SUCCESS;
       }
       else
       {
           hddLog(VOS_TRACE_LEVEL_INFO, "%s:IP Address is not assigned \n", __func__);
           return VOS_STATUS_E_AGAIN;
       }
   }
   else
   {
       vos_mem_zero((void *)&offLoadRequest, sizeof(tSirHostOffloadReq));
       offLoadRequest.enableOrDisable = SIR_OFFLOAD_DISABLE;
       offLoadRequest.offloadType =  SIR_IPV4_ARP_REPLY_OFFLOAD;

       if (eHAL_STATUS_SUCCESS != sme_SetHostOffload(WLAN_HDD_GET_HAL_CTX(pAdapter), pAdapter->sessionId, 
                                                     &offLoadRequest))
       {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Failure to disable host "
                             "offload feature\n", __func__);
            return VOS_STATUS_E_FAILURE;
       }
       return VOS_STATUS_SUCCESS;
   }
}

/*
 * This function is called before setting mcbc filters
 * to modify filter value considering ARP
*/
void hdd_mcbc_filter_modification(hdd_context_t* pHddCtx, v_BOOL_t arpFlag,
                                  tANI_U8 *pMcBcFilter)
{
    if (TRUE == arpFlag)
    {
        /*ARP offload is enabled, do not block bcast packets at RXP*/
        if (pHddCtx->dynamic_mcbc_filter.enableCfg)
        {
            if ((HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST_BROADCAST ==
                  pHddCtx->dynamic_mcbc_filter.mcastBcastFilterSetting))
            {
                *pMcBcFilter = HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST;
            }
            else if ((HDD_MCASTBCASTFILTER_FILTER_ALL_BROADCAST ==
                  pHddCtx->dynamic_mcbc_filter.mcastBcastFilterSetting))
            {
                *pMcBcFilter = HDD_MCASTBCASTFILTER_FILTER_NONE;
            }
            else
            {
                *pMcBcFilter = pHddCtx->dynamic_mcbc_filter.mcastBcastFilterSetting;
            }

            pHddCtx->dynamic_mcbc_filter.enableSuspend = TRUE;
            pHddCtx->dynamic_mcbc_filter.mcBcFilterSuspend = *pMcBcFilter;
        }
        else
        {
            if (HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST_BROADCAST ==
                  pHddCtx->cfg_ini->mcastBcastFilterSetting)
            {
                *pMcBcFilter = HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST;
            }
            else if (HDD_MCASTBCASTFILTER_FILTER_ALL_BROADCAST ==
                  pHddCtx->cfg_ini->mcastBcastFilterSetting)
            {
                *pMcBcFilter = HDD_MCASTBCASTFILTER_FILTER_NONE;
            }
            else
            {
                *pMcBcFilter = pHddCtx->cfg_ini->mcastBcastFilterSetting;
            }

            pHddCtx->dynamic_mcbc_filter.enableSuspend = FALSE;
        }
    }
    else
    {
        if (pHddCtx->dynamic_mcbc_filter.enableCfg)
        {
            *pMcBcFilter = pHddCtx->dynamic_mcbc_filter.mcastBcastFilterSetting;
            pHddCtx->dynamic_mcbc_filter.enableSuspend = TRUE;
        }
        else
        {
            pHddCtx->dynamic_mcbc_filter.enableSuspend = FALSE;
            *pMcBcFilter = pHddCtx->cfg_ini->mcastBcastFilterSetting;
        }
    }
}

void hdd_conf_mcastbcast_filter(hdd_context_t* pHddCtx, v_BOOL_t setfilter)
{
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    tpSirWlanSetRxpFilters wlanRxpFilterParam =
                     vos_mem_malloc(sizeof(tSirWlanSetRxpFilters));
    if(NULL == wlanRxpFilterParam)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
           "%s: vos_mem_alloc failed ", __func__);
        return;
    }
    hddLog(VOS_TRACE_LEVEL_INFO,
        "%s: Configuring Mcast/Bcast Filter Setting. setfilter %d", __func__, setfilter);
    if (TRUE == setfilter)
    {
        if (pHddCtx->cfg_ini->fhostArpOffload)
        {
            hdd_mcbc_filter_modification(pHddCtx, TRUE,
                  &wlanRxpFilterParam->configuredMcstBcstFilterSetting);
        }
        else
        {
            hdd_mcbc_filter_modification(pHddCtx, FALSE,
                  &wlanRxpFilterParam->configuredMcstBcstFilterSetting);
        }
    }
    else
        wlanRxpFilterParam->configuredMcstBcstFilterSetting =
                              pHddCtx->cfg_ini->mcastBcastFilterSetting;

    wlanRxpFilterParam->setMcstBcstFilter = setfilter;
    halStatus = sme_ConfigureRxpFilter(pHddCtx->hHal, wlanRxpFilterParam);
    if (eHAL_STATUS_SUCCESS != halStatus)
        vos_mem_free(wlanRxpFilterParam);
    if(setfilter && (eHAL_STATUS_SUCCESS == halStatus))
       pHddCtx->hdd_mcastbcast_filter_set = TRUE;
}

static void hdd_conf_suspend_ind(hdd_context_t* pHddCtx,
                                 hdd_adapter_t *pAdapter)
{
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    VOS_STATUS vstatus = VOS_STATUS_E_FAILURE;
    tpSirWlanSuspendParam wlanSuspendParam =
      vos_mem_malloc(sizeof(tSirWlanSuspendParam));

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
        if((pHddCtx->cfg_ini->fhostArpOffload) && 
           (eConnectionState_Associated == 
            (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState)) 
        {
            vstatus = hdd_conf_hostarpoffload(pAdapter, TRUE);
            if (!VOS_IS_STATUS_SUCCESS(vstatus))
            {
                hdd_mcbc_filter_modification(pHddCtx, FALSE,
                      &wlanSuspendParam->configuredMcstBcstFilterSetting);
                hddLog(VOS_TRACE_LEVEL_INFO,
                       "%s:Failed to enable ARPOFFLOAD Feature %d\n",
                       __func__, vstatus);
            }
            else
            {
                hdd_mcbc_filter_modification(pHddCtx, TRUE,
                      &wlanSuspendParam->configuredMcstBcstFilterSetting);
            }
        }
        else
        {
            hdd_mcbc_filter_modification(pHddCtx, FALSE,
                      &wlanSuspendParam->configuredMcstBcstFilterSetting);
            if(pHddCtx->dynamic_mcbc_filter.enableCfg)
            {
                pHddCtx->dynamic_mcbc_filter.mcBcFilterSuspend = 
                        wlanSuspendParam->configuredMcstBcstFilterSetting;
            }
        }

#ifdef WLAN_FEATURE_PACKET_FILTERING
        if (pHddCtx->cfg_ini->isMcAddrListFilter)
        {
           /*Multicast addr list filter is enabled during suspend*/
           if (((pAdapter->device_mode == WLAN_HDD_INFRA_STATION) || 
                    (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT))
                 && pAdapter->mc_addr_list.mc_cnt
                 && (eConnectionState_Associated == 
                    (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState))
           {
              /*set the filter*/
              wlan_hdd_set_mc_addr_list(pAdapter, TRUE);
           }
        }
#endif
    }

    halStatus = sme_ConfigureSuspendInd(pHddCtx->hHal, wlanSuspendParam);
    if(eHAL_STATUS_SUCCESS == halStatus)
    {
        pHddCtx->hdd_mcastbcast_filter_set = TRUE;
    } else {
        vos_mem_free(wlanSuspendParam);
    }
}

static void hdd_conf_resume_ind(hdd_adapter_t *pAdapter)
{
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    VOS_STATUS vstatus;
    hdd_context_t* pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    tpSirWlanResumeParam wlanResumeParam;

    hddLog(VOS_TRACE_LEVEL_INFO,
      "%s: send wlan resume indication", __func__);

    if (pHddCtx->hdd_mcastbcast_filter_set == TRUE)
    {
        wlanResumeParam = vos_mem_malloc(sizeof(tSirWlanResumeParam));

        if(NULL == wlanResumeParam)
        {
            hddLog(VOS_TRACE_LEVEL_FATAL,
               "%s: vos_mem_alloc failed ", __func__);
            return;
        }

        // adding the check for association here
        if ((pHddCtx->cfg_ini->fhostArpOffload) &&
                (eConnectionState_Associated ==
                 (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState))
        {
            vstatus = hdd_conf_hostarpoffload(pAdapter, FALSE);
            if (!VOS_IS_STATUS_SUCCESS(vstatus))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR, "%s:Failed to disable ARPOFFLOAD "
                      "Feature %d\n", __func__, vstatus);
            }
        }
        if (pHddCtx->dynamic_mcbc_filter.enableSuspend)
        {
            wlanResumeParam->configuredMcstBcstFilterSetting =
                                   pHddCtx->dynamic_mcbc_filter.mcBcFilterSuspend;
        }
        else
        {
            wlanResumeParam->configuredMcstBcstFilterSetting =
                                        pHddCtx->cfg_ini->mcastBcastFilterSetting;
        }
        halStatus = sme_ConfigureResumeReq(pHddCtx->hHal, wlanResumeParam);
        if (eHAL_STATUS_SUCCESS != halStatus)
            vos_mem_free(wlanResumeParam);
        pHddCtx->hdd_mcastbcast_filter_set = FALSE;
    }


#ifdef WLAN_FEATURE_PACKET_FILTERING    
    if (pHddCtx->cfg_ini->isMcAddrListFilter)
    {
       /*Multicast addr filtering is enabled*/
       if (pAdapter->mc_addr_list.isFilterApplied)
       {
          /*Filter applied during suspend mode*/
          /*Clear it here*/
          wlan_hdd_set_mc_addr_list(pAdapter, FALSE);
       }
    }
#endif
}

//Suspend routine registered with Android OS
void hdd_suspend_wlan(void)
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

       {  // we skip this registration for modes other than STA, SAP and P2P client modes.
           status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
           pAdapterNode = pNext;
           continue;
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
       /* Suspend notification sent down to driver*/
       hdd_conf_suspend_ind(pHddCtx, pAdapter);

#ifdef WLAN_FEATURE_GTK_OFFLOAD
       if ((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) ||
           (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode))
       {
           eHalStatus ret;
           hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
           if ((eConnectionState_Associated == pHddStaCtx->conn_info.connState) &&
                    (TRUE == pHddStaCtx->gtkOffloadRequestParams.requested))
           {
               ret = sme_SetGTKOffload(WLAN_HDD_GET_HAL_CTX(pAdapter),
                              &pHddStaCtx->gtkOffloadRequestParams.gtkOffloadReqParams,
                              pAdapter->sessionId);
               if (eHAL_STATUS_SUCCESS != ret)
               {
                   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                           "%s: sme_SetGTKOffload failed, returned %d",
                           __func__, ret);
               }
               pHddStaCtx->gtkOffloadRequestParams.requested = FALSE;
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                       "%s: sme_SetGTKOffload successfull",
                       __func__);
           }
       }
#endif
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
   if((newState == BMPS) &&  pHddCtx->hdd_wlan_suspended) {
      spin_unlock(&pHddCtx->filter_lock);
      hdd_conf_mcastbcast_filter(pHddCtx, TRUE);
      if(pHddCtx->hdd_mcastbcast_filter_set != TRUE)
         hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Not able to set mcast/bcast filter ", __func__);
   }
   else 
      spin_unlock(&pHddCtx->filter_lock);
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
      pmcRegisterDeviceStateUpdateInd(smeContext,
                                      hdd_PowerStateChangedCB, pHddCtx);
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
      pmcDeregisterDeviceStateUpdateInd(smeContext, hdd_PowerStateChangedCB);
   }
}
#ifdef WLAN_FEATURE_GTK_OFFLOAD
void  wlan_hdd_update_and_dissable_gtk_offload(hdd_adapter_t *pAdapter)
{
    eHalStatus ret;
    tpSirGtkOffloadParams pGtkOffloadReqParams;
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    pGtkOffloadReqParams =
                 &pHddStaCtx->gtkOffloadRequestParams.gtkOffloadReqParams;

    if ((eConnectionState_Associated == pHddStaCtx->conn_info.connState) &&
        (0 !=  memcmp(&pGtkOffloadReqParams->bssId,
                     &pHddStaCtx->conn_info.bssId, WNI_CFG_BSSID_LEN)) &&
        (FALSE == pHddStaCtx->gtkOffloadRequestParams.requested))
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
        }
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s: sme_GetGTKOffload successfull", __func__);

        /* Sending GTK offload dissable */
        pGtkOffloadReqParams->ulFlags = GTK_OFFLOAD_DISABLE;
        ret = sme_SetGTKOffload(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                pGtkOffloadReqParams, pAdapter->sessionId);

        if (eHAL_STATUS_SUCCESS != ret)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s: failed to dissable GTK offload, returned %d",
                    __func__, ret);
        }
        pHddStaCtx->gtkOffloadRequestParams.requested = FALSE;
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s: successfully dissabled GTK offload request to HAL",
                __func__);
    }
}
#endif /*WLAN_FEATURE_GTK_OFFLOAD*/

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
   
   if (pHddCtx->isLogpInProgress) {
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
       {  // we skip this registration for modes other than STA, SAP and P2P client modes.
            status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
            pAdapterNode = pNext;
            continue;
       }

#ifdef WLAN_FEATURE_GTK_OFFLOAD
       if ((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) ||
           (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode))
       {
           wlan_hdd_update_and_dissable_gtk_offload(pAdapter);
       }
#endif

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
                        "Switch to DTIM%d \n",powerRequest.uListenInterval);
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

      hdd_conf_resume_ind(pAdapter);
      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

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
   hdd_prevent_suspend();

   return VOS_STATUS_SUCCESS;
}


/*
 * Based on the ioctl command recieved by HDD, put WLAN driver
 * into the quiet mode. This is the same as the early suspend
 * notification that driver used to listen
 */
void hdd_set_wlan_suspend_mode(bool suspend)
{
    if (suspend)
        hdd_suspend_wlan();
    else
        hdd_resume_wlan();
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
    hddLog(VOS_TRACE_LEVEL_FATAL, "%s: HDD SSR timer expired", __func__);
    VOS_BUG(0);
}

static void hdd_ssr_timer_start(int msec)
{
    if(ssr_timer_started)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL, "%s: trying to start SSR timer when it's running"
                ,__func__);
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

   /* if re-init never happens, then do SSR1 */
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
   hdd_reset_all_adapters(pHddCtx);
   /* DeRegister with platform driver as client for Suspend/Resume */
   vosStatus = hddDeregisterPmOps(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hddDeregisterPmOps failed",__func__);
   }

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
   if(TRUE == pHddCtx->isTxThreadSuspended){
      complete(&vosSchedContext->ResumeTxEvent);
      pHddCtx->isTxThreadSuspended= FALSE;
   }
   if(TRUE == pHddCtx->isRxThreadSuspended){
      complete(&vosSchedContext->ResumeRxEvent);
      pHddCtx->isRxThreadSuspended= FALSE;
   }
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
   wait_for_completion_interruptible(&vosSchedContext->McShutdown);

   /* Wait for TX to exit */
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Shutting down TX thread",__func__);
   set_bit(TX_SHUTDOWN_EVENT_MASK, &vosSchedContext->txEventFlag);
   set_bit(TX_POST_EVENT_MASK, &vosSchedContext->txEventFlag);
   wake_up_interruptible(&vosSchedContext->txWaitQueue);
   wait_for_completion_interruptible(&vosSchedContext->TxShutdown);

   /* Wait for RX to exit */
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Shutting down RX thread",__func__);
   set_bit(RX_SHUTDOWN_EVENT_MASK, &vosSchedContext->rxEventFlag);
   set_bit(RX_POST_EVENT_MASK, &vosSchedContext->rxEventFlag);
   wake_up_interruptible(&vosSchedContext->rxWaitQueue);
   wait_for_completion_interruptible(&vosSchedContext->RxShutdown);

#ifdef WLAN_BTAMP_FEATURE
   vosStatus = WLANBAP_Stop(pVosContext);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to stop BAP",__func__);
   }
#endif //WLAN_BTAMP_FEATURE
   vosStatus = vos_wda_shutdown(pVosContext);
   VOS_ASSERT(VOS_IS_STATUS_SUCCESS(vosStatus));

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing SME STOP",__func__);
   /* Stop SME - Cannot invoke vos_stop as vos_stop relies
    * on threads being running to process the SYS Stop
    */
   vosStatus = sme_Stop(pHddCtx->hHal, TRUE);
   VOS_ASSERT(VOS_IS_STATUS_SUCCESS(vosStatus));

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing MAC STOP",__func__);
   /* Stop MAC (PE and HAL) */
   vosStatus = macStop(pHddCtx->hHal, HAL_STOP_TYPE_SYS_RESET);
   VOS_ASSERT(VOS_IS_STATUS_SUCCESS(vosStatus));

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing TL STOP",__func__);
   /* Stop TL */
   vosStatus = WLANTL_Stop(pVosContext);
   VOS_ASSERT(VOS_IS_STATUS_SUCCESS(vosStatus));

   hdd_unregister_mcast_bcast_filter(pHddCtx);
   hddLog(VOS_TRACE_LEVEL_INFO, "%s: Flush Queues",__func__);
   /* Clean up message queues of TX and MC thread */
   vos_sched_flush_mc_mqs(vosSchedContext);
   vos_sched_flush_tx_mqs(vosSchedContext);
   vos_sched_flush_rx_mqs(vosSchedContext);

   /* Deinit all the TX and MC queues */
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
VOS_STATUS hdd_wlan_re_init(void)
{
   VOS_STATUS       vosStatus;
   v_CONTEXT_t      pVosContext = NULL;
   hdd_context_t    *pHddCtx = NULL;
   eHalStatus       halStatus;
#ifdef HAVE_WCNSS_CAL_DOWNLOAD
   int              max_retries = 0;
#endif
#ifdef WLAN_BTAMP_FEATURE
   hdd_config_t     *pConfig = NULL;
   WLANBAP_ConfigType btAmpConfig;
#endif

   hdd_ssr_timer_del();
   hdd_prevent_suspend();

#ifdef HAVE_WCNSS_CAL_DOWNLOAD
   /* wait until WCNSS driver downloads NV */
   while (!wcnss_device_ready() && 5 >= ++max_retries) {
       msleep(1000);
   }
   if (max_retries >= 5) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: WCNSS driver not ready", __func__);
      goto err_re_init;
   }
#endif

   /* The driver should always be initialized in STA mode after SSR */
   hdd_set_conparam(0);

   /* Re-open VOSS, it is a re-open b'se control transport was never closed. */
   vosStatus = vos_open(&pVosContext, 0);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_open failed",__func__);
      goto err_re_init;
   }

   /* Get the HDD context. */
   pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext);
   if(!pHddCtx)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD context is Null",__func__);
      goto err_vosclose;
   }

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

   /* Initialize the WMM module */
   vosStatus = hdd_wmm_init(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: hdd_wmm_init failed", __func__);
      goto err_vosclose;
   }

   vosStatus = vos_preStart( pHddCtx->pvosContext );
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_preStart failed",__func__);
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

   /* Exchange capability info between Host and FW and also get versioning info from FW */
   hdd_exchange_version_and_caps(pHddCtx);

   vosStatus = hdd_post_voss_start_config( pHddCtx );
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hdd_post_voss_start_config failed",
         __func__);
      goto err_vosstop;
   }

#ifdef WLAN_BTAMP_FEATURE
   vosStatus = WLANBAP_Open(pVosContext);
   if(!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: Failed to open BAP",__func__);
      goto err_vosstop;
   }
   vosStatus = BSL_Init(pVosContext);
   if(!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: Failed to Init BSL",__func__);
     goto err_bap_close;
   }
   vosStatus = WLANBAP_Start(pVosContext);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to start TL",__func__);
       goto err_bap_close;
   }
   pConfig = pHddCtx->cfg_ini;
   btAmpConfig.ucPreferredChannel = pConfig->preferredChannel;
   vosStatus = WLANBAP_SetConfig(&btAmpConfig);
#endif //WLAN_BTAMP_FEATURE

    /* Restart all adapters */
   hdd_start_all_adapters(pHddCtx);
   pHddCtx->isLogpInProgress = FALSE;
   pHddCtx->hdd_mcastbcast_filter_set = FALSE;
   hdd_register_mcast_bcast_filter(pHddCtx);

   /* Register with platform driver as client for Suspend/Resume */
   vosStatus = hddRegisterPmOps(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hddRegisterPmOps failed",__func__);
      goto err_bap_stop;
   }
   /* Allow the phone to go to sleep */
   hdd_allow_suspend();
   /* register for riva power on lock */
   if (req_riva_power_on_lock("wlan"))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: req riva power on lock failed",
                                        __func__);
      goto err_unregister_pmops;
   }
   goto success;

err_unregister_pmops:
   hddDeregisterPmOps(pHddCtx);

err_bap_stop:
#ifdef CONFIG_HAS_EARLYSUSPEND
   hdd_unregister_mcast_bcast_filter(pHddCtx);
#endif
   hdd_close_all_adapters(pHddCtx);
#ifdef WLAN_BTAMP_FEATURE
   WLANBAP_Stop(pVosContext);
#endif

#ifdef WLAN_BTAMP_FEATURE
err_bap_close:
   WLANBAP_Close(pVosContext);
#endif

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
       nl_srv_exit();
       /* Free up dynamically allocated members inside HDD Adapter */
       kfree(pHddCtx->cfg_ini);
       pHddCtx->cfg_ini= NULL;

       wiphy_unregister(pHddCtx->wiphy);
       wiphy_free(pHddCtx->wiphy);
   }
   vos_preClose(&pVosContext);

#ifdef MEMORY_DEBUG
   vos_mem_exit();
#endif

err_re_init:
   /* Allow the phone to go to sleep */
   hdd_allow_suspend();
   VOS_BUG(0);
   return -EPERM;

success:
   /* Trigger replay of BTC events */
   send_btc_nlink_msg(WLAN_MODULE_DOWN_IND, 0);
   return VOS_STATUS_SUCCESS;
}
