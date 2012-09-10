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
#ifdef CONFIG_HAS_EARLYSUSPEND

#include <linux/pm.h>
#include <linux/wait.h>
#include <linux/earlysuspend.h>
#include <wlan_hdd_includes.h>
#include <wlan_qct_driver.h>
#include <linux/wakelock.h>
#endif

#include "halTypes.h"
#include "sme_Api.h"
#include <vos_api.h>
#include "vos_power.h"
#include <vos_sched.h>
#include <macInitApi.h>
#ifdef ANI_BUS_TYPE_SDIO
#include <wlan_qct_sal.h>
#include <wlan_qct_bal.h>
#endif
#include <wlan_qct_sys.h>
#include <wlan_btc_svc.h>
#include <wlan_nlink_common.h>
#include <wlan_hdd_main.h>
#include <wlan_hdd_assoc.h>
#include <wlan_hdd_dev_pwr.h>
#ifdef ANI_BUS_TYPE_SDIO
#include <wlan_sal_misc.h>
#include <libra_sdioif.h>
#endif
#include <wlan_nlink_srv.h>
#include <wlan_hdd_misc.h>

#ifdef WLAN_SOFTAP_FEATURE
#include <linux/semaphore.h>
#include <wlan_hdd_hostapd.h>
#endif
#include "cfgApi.h"

#ifdef WLAN_BTAMP_FEATURE
#include "bapApi.h"
#include "bap_hdd_main.h"
#include "bap_hdd_misc.h"
#endif

#include <linux/wcnss_wlan.h>
#include <linux/inetdevice.h>
#include <wlan_hdd_cfg.h>
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

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend wlan_early_suspend;
#endif

static eHalStatus g_full_pwr_status;
static eHalStatus g_standby_status;

extern VOS_STATUS hdd_post_voss_start_config(hdd_context_t* pHddCtx);
extern VOS_STATUS vos_chipExitDeepSleepVREGHandler(
   vos_call_status_type* status,
   vos_power_cb_type callback,
   v_PVOID_t user_data);
extern void hdd_wlan_initial_scan(hdd_context_t *pHddCtx);
void unregister_wlan_suspend(void);

extern struct notifier_block hdd_netdev_notifier;
#ifdef WLAN_SOFTAP_FEATURE
extern tVOS_CON_MODE hdd_get_conparam ( void );
#endif

#ifdef WLAN_FEATURE_PACKET_FILTERING
extern void wlan_hdd_set_mc_addr_list(hdd_context_t *pHddCtx, v_U8_t set, v_U8_t sessionId);
#endif

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
#ifdef ANI_BUS_TYPE_SDIO
   struct sdio_func *sdio_func_dev_current = NULL;
   int attempts = 0;
#endif
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

#ifdef ANI_BUS_TYPE_SDIO
   vosStatus = WLANBAL_Stop( pHddCtx->pvosContext );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );

   vosStatus = WLANBAL_SuspendChip( pHddCtx->pvosContext );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );

   vosStatus = WLANSAL_Stop(pHddCtx->pvosContext);
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
#endif

   vosStatus = vos_chipAssertDeepSleep( &callType, NULL, NULL );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );

   //Vote off any PMIC voltage supplies
   vosStatus = vos_chipPowerDown(NULL, NULL, NULL);

#ifdef ANI_BUS_TYPE_SDIO
    //Get the Current SDIO Func
   sdio_func_dev_current = libra_getsdio_funcdev();
   if(NULL != sdio_func_dev_current) {
      libra_detect_card_change();
      do {
         msleep(100);
         //Get the SDIO func device
         sdio_func_dev_current = libra_getsdio_funcdev();
         if(NULL == sdio_func_dev_current) {
            hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Card Removed Successfully",__func__);
            break;
         }
         else {
            hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Failed to Remove the Card: Trying Again",__func__);
            attempts++;
         }
      } while (attempts < LIBRA_CARD_REMOVE_DETECT_MAX_COUNT);

      if(LIBRA_CARD_REMOVE_DETECT_MAX_COUNT == attempts) {
         hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Failed to Remove the Card: Fatal",__func__);
         goto err_fail;
      }
   }
#endif

   pHddCtx->hdd_ps_state = eHDD_SUSPEND_DEEP_SLEEP;

   //Restore IMPS config
   if(pHddCtx->cfg_ini->fIsImpsEnabled)
      sme_EnablePowerSave(pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);

   //Restore BMPS config
   if(pHddCtx->cfg_ini->fIsBmpsEnabled)
      sme_EnablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);

#ifdef ANI_BUS_TYPE_SDIO
err_fail:
#endif
   return vosStatus;
}

VOS_STATUS hdd_exit_deep_sleep(hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter)
{
   VOS_STATUS vosStatus;
   eHalStatus halStatus;
#ifdef ANI_BUS_TYPE_SDIO
   int attempts = 0;
   struct sdio_func *sdio_func_dev = NULL;
#endif

   //Power Up Libra WLAN card first if not already powered up
   vosStatus = vos_chipPowerUp(NULL,NULL,NULL);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Libra WLAN not Powered Up. "
          "exiting", __func__);
      goto err_deep_sleep;
   }

#ifdef ANI_BUS_TYPE_SDIO
   libra_detect_card_change();

   do {
      sdio_func_dev = libra_getsdio_funcdev();
      if (NULL == sdio_func_dev) {
         hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Libra WLAN not detected yet.",__func__);
         attempts++;
      }
      else {
         hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Libra WLAN detecton succeeded",__func__);
         break;
      }

      if(LIBRA_CARD_INSERT_DETECT_MAX_COUNT == attempts)
        break;

      msleep(250);

   }while (attempts < LIBRA_CARD_INSERT_DETECT_MAX_COUNT);

   //Retry to detect the card again by Powering Down the chip and Power up the chip
   //again. This retry is done to recover from CRC Error
   if (NULL == sdio_func_dev) {

      attempts = 0;

      //Vote off any PMIC voltage supplies
      vos_chipPowerDown(NULL, NULL, NULL);

      msleep(1000);

      //Power Up Libra WLAN card first if not already powered up
      vosStatus = vos_chipPowerUp(NULL,NULL,NULL);
      if (!VOS_IS_STATUS_SUCCESS(vosStatus))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Retry Libra WLAN not Powered Up. "
             "exiting", __func__);
         goto err_deep_sleep;
      }

      do {
         sdio_func_dev = libra_getsdio_funcdev();
         if (NULL == sdio_func_dev) {
            hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Retry Libra WLAN not detected yet.",__func__);
            attempts++;
         }
         else {
            hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Retry Libra WLAN detecton succeeded",__func__);
            break;
         }

         if(attempts == 2)
           break;

         msleep(1000);

      }while (attempts < 3);

   }

   if (NULL == sdio_func_dev) {
         hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Libra WLAN not found!!",__func__);
         goto err_deep_sleep;
   }

   SET_NETDEV_DEV(pAdapter->dev, &sdio_func_dev->dev);
   libra_sdio_setprivdata (sdio_func_dev, pHddCtx);
   atomic_set(&pHddCtx->sdio_claim_count, 0);
   pHddCtx->parent_dev = &sdio_func_dev->dev;

   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, 
      "%s: calling WLANSAL_Start",__func__);
   vosStatus = WLANSAL_Start(pHddCtx->pvosContext);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to start SAL",__func__);
      goto err_deep_sleep;
   }

   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
      "%s: calling WLANBAL_ResumeChip",__func__);
   vosStatus = WLANBAL_ResumeChip( pHddCtx->pvosContext );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed in WLANBAL_ResumeChip",__func__);
      goto err_sal_stop;
   }

   vosStatus = WLANBAL_Start(pHddCtx->pvosContext);

   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
               "%s: Failed to start BAL",__func__);
      goto err_sal_stop;
   }
#endif

   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
      "%s: calling hdd_set_sme_config",__func__);
   vosStatus = hdd_set_sme_config( pHddCtx );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed in hdd_set_sme_config",__func__);
#ifdef ANI_BUS_TYPE_SDIO
      goto err_bal_stop;
#else
      goto err_deep_sleep;
#endif
   }

   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, 
      "%s: calling vos_start",__func__);
   vosStatus = vos_start( pHddCtx->pvosContext );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed in vos_start",__func__);
#ifdef ANI_BUS_TYPE_SDIO
      goto err_bal_stop;
#else
      goto err_deep_sleep;
#endif
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
#ifdef ANI_BUS_TYPE_SDIO
err_bal_stop:
   WLANBAL_Stop(pHddCtx->pvosContext);
err_sal_stop:
   WLANSAL_Stop(pHddCtx->pvosContext);
#endif
err_deep_sleep:
   return VOS_STATUS_E_FAILURE;

}

VOS_STATUS hdd_conf_hostarpoffload(hdd_context_t* pHddCtx, v_BOOL_t fenable)
{
   struct in_ifaddr **ifap = NULL;
   struct in_ifaddr *ifa = NULL;
   struct in_device *in_dev;
   int i = 0;
   hdd_adapter_t *pAdapter = NULL;   
   tSirHostOffloadReq  offLoadRequest;

   hddLog(VOS_TRACE_LEVEL_ERROR, "%s: \n", __func__);
#ifdef WLAN_FEATURE_PACKET_FILTERING
   if (pHddCtx->cfg_ini->isMcAddrListFilter)
   {
      pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_P2P_GO);
      if (pAdapter != NULL)
      {
         VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: Can't set multicast addr filtering in P2P-GO HDD", __FUNCTION__);
         return VOS_STATUS_E_FAILURE;
      }

      pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_P2P_CLIENT);
      if (pAdapter == NULL)    
         pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_INFRA_STATION);

      if(pAdapter == NULL)
      {
         VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,"%s: HDD adapter context is Null", __FUNCTION__);
         return VOS_STATUS_E_FAILURE;
      }
   }
   else
   {
#endif
      pAdapter = hdd_get_adapter(pHddCtx,WLAN_HDD_INFRA_STATION);
      if(pAdapter == NULL)
      {
         pAdapter = hdd_get_adapter(pHddCtx,WLAN_HDD_P2P_CLIENT);
         if(pAdapter == NULL)
         {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,"%s: HDD adapter context is Null", __FUNCTION__);
            return VOS_STATUS_E_FAILURE;
         }
      }
#ifdef WLAN_FEATURE_PACKET_FILTERING
   }
#endif
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

           if((HDD_MCASTBCASTFILTER_FILTER_ALL_BROADCAST ==
                   pHddCtx->cfg_ini->mcastBcastFilterSetting )
                    || (HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST_BROADCAST ==
                    pHddCtx->cfg_ini->mcastBcastFilterSetting))
           {
               //MCAST filter is set by hdd_conf_mcastbcast_filter fn call
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

void hdd_conf_mcastbcast_filter(hdd_context_t* pHddCtx, v_BOOL_t setfilter)
{
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
    tpAniSirGlobal pMac = (tpAniSirGlobal) vos_get_context(VOS_MODULE_ID_SME, pHddCtx->pvosContext);
#else
    tpSirWlanSetRxpFilters wlanRxpFilterParam =
                     vos_mem_malloc(sizeof(tSirWlanSetRxpFilters));
    if(NULL == wlanRxpFilterParam)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
           "%s: vos_mem_alloc failed ", __func__);
        return;
    }
#endif
    hddLog(VOS_TRACE_LEVEL_INFO,
        "%s: Configuring Mcast/Bcast Filter Setting. setfilter %d", __func__, setfilter);
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
    if ( pMac ) 
    {
      halStatus = halRxp_configureRxpFilterMcstBcst( pMac, setfilter);
    }
    else
    {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: pMac is initialised to NULL",__func__ );
    }
#else
    wlanRxpFilterParam->setMcstBcstFilter = setfilter;
    wlanRxpFilterParam->configuredMcstBcstFilterSetting = 
                      pHddCtx->cfg_ini->mcastBcastFilterSetting;
    halStatus = sme_ConfigureRxpFilter(pHddCtx->hHal, wlanRxpFilterParam);
#endif
    if(setfilter && (eHAL_STATUS_SUCCESS == halStatus))
       pHddCtx->hdd_mcastbcast_filter_set = TRUE;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
#ifdef FEATURE_WLAN_INTEGRATED_SOC
static void hdd_conf_suspend_ind(hdd_context_t* pHddCtx,
                                 hdd_adapter_t *pAdapter)
{
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    VOS_STATUS vstatus;
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
            vstatus = hdd_conf_hostarpoffload(pHddCtx, TRUE);
            if (!VOS_IS_STATUS_SUCCESS(vstatus))
            {
                if(pHddCtx->dynamic_mcbc_filter.enableCfg)
                {
                   wlanSuspendParam->configuredMcstBcstFilterSetting = 
                               pHddCtx->dynamic_mcbc_filter.mcastBcastFilterSetting;
                }
                else
                {
                  wlanSuspendParam->configuredMcstBcstFilterSetting = 
                             pHddCtx->cfg_ini->mcastBcastFilterSetting;
                }
                hddLog(VOS_TRACE_LEVEL_INFO,
                       "%s:Failed to enable ARPOFFLOAD Feature %d\n",
                       __func__, vstatus);
            }
            else
            {
               if (((pHddCtx->dynamic_mcbc_filter.enableCfg) && 
                    (HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST_BROADCAST == 
                    pHddCtx->dynamic_mcbc_filter.mcastBcastFilterSetting)) ||
                    (HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST_BROADCAST == 
                    pHddCtx->cfg_ini->mcastBcastFilterSetting))
               {
                 wlanSuspendParam->configuredMcstBcstFilterSetting = 
                                 HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST;
               }
               else if(((pHddCtx->dynamic_mcbc_filter.enableCfg) &&
                        (HDD_MCASTBCASTFILTER_FILTER_ALL_BROADCAST == 
                        pHddCtx->dynamic_mcbc_filter.mcastBcastFilterSetting)) ||
                        (HDD_MCASTBCASTFILTER_FILTER_ALL_BROADCAST == 
                        pHddCtx->cfg_ini->mcastBcastFilterSetting))
               {
                 wlanSuspendParam->configuredMcstBcstFilterSetting = 
                                         HDD_MCASTBCASTFILTER_FILTER_NONE;
               }
               else
               {
                 if(pHddCtx->dynamic_mcbc_filter.enableCfg) 
                 {
                   wlanSuspendParam->configuredMcstBcstFilterSetting = 
                               pHddCtx->dynamic_mcbc_filter.mcastBcastFilterSetting;
                 }
                 else
                 {
                   wlanSuspendParam->configuredMcstBcstFilterSetting = 
                             pHddCtx->cfg_ini->mcastBcastFilterSetting;
                 }
               }
           }
        }
        else
        {
           if(pHddCtx->dynamic_mcbc_filter.enableCfg)
           {
              wlanSuspendParam->configuredMcstBcstFilterSetting = 
                      pHddCtx->dynamic_mcbc_filter.mcastBcastFilterSetting;
           }
           else
           {
             wlanSuspendParam->configuredMcstBcstFilterSetting = 
                             pHddCtx->cfg_ini->mcastBcastFilterSetting;
           }
        }
        
        pHddCtx->dynamic_mcbc_filter.enableSuspend = TRUE;
        pHddCtx->dynamic_mcbc_filter.mcBcFilterSuspend = 
                        wlanSuspendParam->configuredMcstBcstFilterSetting;


#ifdef WLAN_FEATURE_PACKET_FILTERING
        if (pHddCtx->cfg_ini->isMcAddrListFilter)
        {
           /*Multicast addr list filter is enabled during suspend*/
           if (((pAdapter->device_mode == WLAN_HDD_INFRA_STATION) || 
                    (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT))
                 && pHddCtx->mc_addr_list.mc_cnt
                 && (eConnectionState_Associated == 
                    (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState))
           {
              /*set the filter*/
              wlan_hdd_set_mc_addr_list(pHddCtx, TRUE, pAdapter->sessionId);
           }
        }
#endif
    }

    halStatus = sme_ConfigureSuspendInd(pHddCtx->hHal, wlanSuspendParam);
    if(eHAL_STATUS_SUCCESS == halStatus)
    {
        pHddCtx->hdd_mcastbcast_filter_set = TRUE;
    }
}

static void hdd_conf_resume_ind(hdd_context_t* pHddCtx, v_U8_t sessionId)
{
    VOS_STATUS vstatus;
    tpSirWlanResumeParam wlanResumeParam =
      vos_mem_malloc(sizeof(tSirWlanResumeParam));

    if(NULL == wlanResumeParam)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
           "%s: vos_mem_alloc failed ", __func__);
        return;
    }

    hddLog(VOS_TRACE_LEVEL_INFO, 
      "%s: send wlan resume indication", __func__);

    if(pHddCtx->cfg_ini->fhostArpOffload)
    {
        vstatus = hdd_conf_hostarpoffload(pHddCtx, FALSE);
        if (!VOS_IS_STATUS_SUCCESS(vstatus))
        {
            hddLog(VOS_TRACE_LEVEL_INFO, "%s:Failed to disable ARPOFFLOAD "
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
    sme_ConfigureResumeReq(pHddCtx->hHal, wlanResumeParam);

#ifdef WLAN_FEATURE_PACKET_FILTERING    
    if (pHddCtx->cfg_ini->isMcAddrListFilter)
    {
       /*Mutlicast addr filtering is enabled*/
       if(pHddCtx->mc_addr_list.isFilterApplied)
       {
          /*Filter applied during suspend mode*/
          /*Clear it here*/
          wlan_hdd_set_mc_addr_list(pHddCtx, FALSE, sessionId);
       }
    }
#endif
}
#endif

//Suspend routine registered with Android OS
void hdd_suspend_wlan(struct early_suspend *wlan_suspend)
{
   hdd_context_t *pHddCtx = NULL;
   v_CONTEXT_t pVosContext = NULL;

   hdd_adapter_t *pAdapter = NULL; 
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
#ifdef ANI_BUS_TYPE_SDIO
   struct sdio_func *sdio_func_dev = NULL;
#endif
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

#ifdef ANI_BUS_TYPE_SDIO
   sdio_func_dev = libra_getsdio_funcdev();

   if(sdio_func_dev == NULL)
   {
        /* Our card got removed */
        hddLog(VOS_TRACE_LEVEL_FATAL, "%s: sdio_func_dev is NULL!",__func__);
        return;
   }

   if(!sd_is_drvdata_available(sdio_func_dev))
   {
        /* Our card got removed */
        hddLog(VOS_TRACE_LEVEL_FATAL,
               "%s: HDD context is not available in sdio_func_dev!",__func__);
        return;
   }

   sd_claim_host(sdio_func_dev);
   
   // Prevent touching the pMac while LOGP reset in progress, we should never get here
   // as the wake lock is already acquired and it would prevent from entering suspend 
   if (pHddCtx->isLogpInProgress) {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Ignore suspend wlan, LOGP in progress!", __func__);
      sd_release_host(sdio_func_dev);
      return;
   }
#endif

   /*loop through all adapters. TBD fix for Concurrency */
   status =  hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
       pAdapter = pAdapterNode->pAdapter;
       if ( (WLAN_HDD_INFRA_STATION != pAdapter->device_mode)
         && (WLAN_HDD_P2P_CLIENT != pAdapter->device_mode) )

       {  //just do for station interface
           status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
           pAdapterNode = pNext;
           continue;
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

   //Apply Dynamic Dtim For P2P
   //Only if ignoreDynamicDtimInP2pMode is not set in ini
   if((pHddCtx->cfg_ini->enableDynamicDTIM ||
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
  
      /*Back up the actual values from CFG */
      wlan_cfgGetInt(pHddCtx->hHal, WNI_CFG_IGNORE_DTIM, 
                              &pHddCtx->hdd_actual_ignore_DTIM_value);
      wlan_cfgGetInt(pHddCtx->hHal, WNI_CFG_LISTEN_INTERVAL, 
                              &pHddCtx->hdd_actual_LI_value);
      
      if(pHddCtx->cfg_ini->enableModulatedDTIM)
      {
          powerRequest.uDTIMPeriod = pHddCtx->cfg_ini->enableModulatedDTIM;
          powerRequest.uListenInterval = pHddCtx->hdd_actual_LI_value;
      }
      else
      {
          powerRequest.uListenInterval = pHddCtx->cfg_ini->enableDynamicDTIM;
      }   

      /* Update ignoreDTIM and ListedInterval in CFG to remain at the DTIM 
      *specified during Enter/Exit BMPS when LCD off*/
      ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_IGNORE_DTIM, powerRequest.uIgnoreDTIM,
                       NULL, eANI_BOOLEAN_FALSE);
      ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_LISTEN_INTERVAL, powerRequest.uListenInterval, 
                       NULL, eANI_BOOLEAN_FALSE);

      /* switch to the DTIM specified in cfg.ini */
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, 
                     "Switch to DTIM%d \n", powerRequest.uListenInterval);
      sme_SetPowerParams( WLAN_HDD_GET_HAL_CTX(pAdapter), &powerRequest);    

      if (BMPS == pmcGetPmcState(pHddCtx->hHal))
      {
          /* put the device into full power */
          wlan_hdd_enter_bmps(pAdapter, DRIVER_POWER_MODE_ACTIVE);

          /* put the device back into BMPS */
          wlan_hdd_enter_bmps(pAdapter, DRIVER_POWER_MODE_AUTO);

          pHddCtx->hdd_ignore_dtim_enabled = TRUE;
      }
   }

#ifdef FEATURE_WLAN_INTEGRATED_SOC
   /*Suspend notification sent down to driver*/
      hdd_conf_suspend_ind(pHddCtx, pAdapter);
#else
      if(pHddCtx->cfg_ini->nEnableSuspend == WLAN_MAP_SUSPEND_TO_MCAST_BCAST_FILTER) {
         if(eConnectionState_Associated == 
            (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState) {
            hdd_conf_mcastbcast_filter(pHddCtx, TRUE);
            halPSAppsCpuWakeupState(vos_get_context(VOS_MODULE_ID_SME,
                                  pHddCtx->pvosContext), FALSE);
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
#ifdef ANI_BUS_TYPE_SDIO
   sd_release_host(sdio_func_dev);
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
   if((newState == BMPS) &&  pHddCtx->hdd_wlan_suspended
          && (pHddCtx->hdd_mcastbcast_filter_set != TRUE)) {
      spin_unlock(&pHddCtx->filter_lock);
      hdd_conf_mcastbcast_filter(pHddCtx, TRUE);
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
      halPSAppsCpuWakeupState(vos_get_context(VOS_MODULE_ID_SME, pHddCtx->pvosContext), FALSE);
#endif
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
      hddLog(LOGE, "%s: Invalid pContext", __FUNCTION__);
      return;
   }
   smeContext = vos_get_context(VOS_MODULE_ID_SME, pVosContext);
   if (NULL == smeContext)
   {
      hddLog(LOGE, "%s: Invalid smeContext", __FUNCTION__);
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
      hddLog(LOGE, "%s: Invalid pContext", __FUNCTION__);
      return;
   }
   smeContext = vos_get_context(VOS_MODULE_ID_SME, pVosContext);
   if (NULL == smeContext)
   {
      hddLog(LOGE, "%s: Invalid smeContext", __FUNCTION__);
      return;
   }

   if (WLAN_MAP_SUSPEND_TO_MCAST_BCAST_FILTER == 
                                            pHddCtx->cfg_ini->nEnableSuspend)
   {
      pmcDeregisterDeviceStateUpdateInd(smeContext, hdd_PowerStateChangedCB);
   }
}

void hdd_resume_wlan(struct early_suspend *wlan_suspend)
{
   hdd_context_t *pHddCtx = NULL;
   hdd_adapter_t *pAdapter = NULL;
   hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
   VOS_STATUS status;
   v_CONTEXT_t pVosContext = NULL;
#ifdef ANI_BUS_TYPE_SDIO
   struct sdio_func *sdio_func_dev = NULL;
#endif
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

#ifdef ANI_BUS_TYPE_SDIO
   sdio_func_dev = libra_getsdio_funcdev();

   if(sdio_func_dev == NULL)
   {
      /* Our card got removed */
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: sdio_func_dev is NULL!",__func__);
      return;
   }

   if(!sd_is_drvdata_available(sdio_func_dev))
   {
        /* Our card got removed */
        hddLog(VOS_TRACE_LEVEL_FATAL, "%s: HDD context is not available "
                                       "in sdio_func_dev!",__func__);
        return;
   }

   sd_claim_host(sdio_func_dev);
   
   // Prevent touching the pMac while LOGP reset in progress, 
   if (pHddCtx->isLogpInProgress) {
      hddLog(VOS_TRACE_LEVEL_INFO, "%s: Ignore resume wlan, LOGP in progress!", __func__);
      sd_release_host(sdio_func_dev);
      return;
   }
#endif
   pHddCtx->hdd_wlan_suspended = FALSE;
   /*loop through all adapters. Concurrency */
   status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

   while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
   {
       pAdapter = pAdapterNode->pAdapter;
       if ( (WLAN_HDD_INFRA_STATION != pAdapter->device_mode)
         && (WLAN_HDD_P2P_CLIENT != pAdapter->device_mode) )
       {  //just do for station interface
            status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
            pAdapterNode = pNext;
            continue;
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
         sme_SetPowerParams( WLAN_HDD_GET_HAL_CTX(pAdapter), &powerRequest);    

         /* put the device into full power */
         wlan_hdd_enter_bmps(pAdapter, DRIVER_POWER_MODE_ACTIVE);

         /* put the device back into BMPS */
         wlan_hdd_enter_bmps(pAdapter, DRIVER_POWER_MODE_AUTO);

         pHddCtx->hdd_ignore_dtim_enabled = FALSE;
      }

         if(pHddCtx->hdd_mcastbcast_filter_set == TRUE) {
#ifdef FEATURE_WLAN_INTEGRATED_SOC
           hdd_conf_resume_ind(pHddCtx, pAdapter->sessionId);
#else
                  hdd_conf_mcastbcast_filter(pHddCtx, FALSE);
                              pHddCtx->hdd_mcastbcast_filter_set = FALSE;
                  halPSAppsCpuWakeupState(vos_get_context(VOS_MODULE_ID_SME,
                                          pHddCtx->pvosContext), TRUE);
#endif
           pHddCtx->hdd_mcastbcast_filter_set = FALSE;
      }
      status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
      pAdapterNode = pNext;
   }

#ifdef SUPPORT_EARLY_SUSPEND_STANDBY_DEEPSLEEP   
   if(pHddCtx->hdd_ps_state == eHDD_SUSPEND_STANDBY) 
   {
       hdd_exit_standby(pHddCtx);
   }    
#endif

#ifdef ANI_BUS_TYPE_SDIO
   sd_release_host(sdio_func_dev);
#endif
   return;
}

VOS_STATUS hdd_wlan_reset(void) 
{
   VOS_STATUS vosStatus;
   hdd_context_t *pHddCtx = NULL;
   v_CONTEXT_t pVosContext = NULL;
   pVosSchedContext vosSchedContext = NULL;
#ifdef ANI_BUS_TYPE_SDIO
   struct sdio_func *sdio_func_dev_new = NULL;
   struct sdio_func *sdio_func_dev_current = NULL;
   unsigned int attempts = 0;
#endif
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: WLAN being reset",__func__);

   //Get the global VOSS context.
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   if(!pVosContext) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Global VOS context is Null", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   //Get the HDD context.
   pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext);
   if(!pHddCtx) {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD context is Null",__func__);
      return VOS_STATUS_E_FAILURE;
   }
#ifdef ANI_BUS_TYPE_SDIO
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing SAL Stop",__func__);
   //Invoke SAL stop
   vosStatus = WLANSAL_Stop( pVosContext );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
         "%s: Failed to stop SAL",__func__);
      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   }
#endif
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Asserting Deep Sleep",__func__);
   //Assert Deep sleep signal now to put Libra HW in lowest power state
   vosStatus = vos_chipAssertDeepSleep( NULL, NULL, NULL );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Power Down Chip",__func__);   
   //Vote off any PMIC voltage supplies
   vos_chipPowerDown(NULL, NULL, NULL);


   /**
   EVM issue is observed with 1.6Mhz freq for 1.3V supply in wlan standalone case.
   During concurrent operation (e.g. WLAN and WCDMA) this issue is not observed. 
   To workaround, wlan will vote for 3.2Mhz during startup and will vote for 1.6Mhz
   during exit.
   */
   if (vos_chipVoteFreqFor1p3VSupply(NULL, NULL, NULL, VOS_NV_FREQUENCY_FOR_1_3V_SUPPLY_1P6MH) != VOS_STATUS_SUCCESS)
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to set the freq to 1.6Mhz for 1.3V Supply",__func__ );

   //Disable IMPS/BMPS as we do not want the device to enter any power
   //save mode on its own during reset sequence
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);
   sme_DisablePowerSave(pHddCtx->hHal, ePMC_UAPSD_MODE_POWER_SAVE);

   //Kill all the threads first. We do not want any messages
   //to be a processed any more and the best way to ensure that
   //is to terminate the threads gracefully.
   vosSchedContext = get_vos_sched_ctxt();

   /* Wakeup Mc Thread if Suspended */
   if(TRUE == pHddCtx->isMcThreadSuspended){
      complete(&vosSchedContext->ResumeMcEvent);
      pHddCtx->isMcThreadSuspended= FALSE;
   }
   /* Wakeup Tx Thread if Suspended */
   if(TRUE == pHddCtx->isTxThreadSuspended){
      complete(&vosSchedContext->ResumeTxEvent);
      pHddCtx->isTxThreadSuspended= FALSE;
   }
#ifdef FEATURE_WLAN_INTEGRATED_SOC
   /* Wakeup Rx Thread if Suspended */
   if(TRUE == pHddCtx->isRxThreadSuspended){
      complete(&vosSchedContext->ResumeRxEvent);
      pHddCtx->isRxThreadSuspended= FALSE;
   }
#endif

   /* Reset the Suspend Variable */
   pHddCtx->isWlanSuspended = FALSE;

   //Wait for MC to exit
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Shutting down MC thread",__func__);
   set_bit(MC_SHUTDOWN_EVENT_MASK, &vosSchedContext->mcEventFlag);
   set_bit(MC_POST_EVENT_MASK, &vosSchedContext->mcEventFlag);
   wake_up_interruptible(&vosSchedContext->mcWaitQueue);
   wait_for_completion_interruptible(&vosSchedContext->McShutdown);

   //Wait for TX to exit
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Shutting down TX thread",__func__);
   set_bit(TX_SHUTDOWN_EVENT_MASK, &vosSchedContext->txEventFlag);
   set_bit(TX_POST_EVENT_MASK, &vosSchedContext->txEventFlag);
   wake_up_interruptible(&vosSchedContext->txWaitQueue);
   wait_for_completion_interruptible(&vosSchedContext->TxShutdown);

#ifdef FEATURE_WLAN_INTEGRATED_SOC
   //Wait for RX to exit
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Shutting down RX thread",__func__);
   set_bit(RX_SHUTDOWN_EVENT_MASK, &vosSchedContext->rxEventFlag);
   set_bit(RX_POST_EVENT_MASK, &vosSchedContext->rxEventFlag);
   wake_up_interruptible(&vosSchedContext->rxWaitQueue);
   wait_for_completion_interruptible(&vosSchedContext->RxShutdown);
#endif

   /* Cancel the vote for XO Core ON in LOGP since we are reinitializing our driver
    * This is done here to ensure there is no race condition since MC and TX thread have
    * exited at this point
    */
   hddLog(VOS_TRACE_LEVEL_WARN, "In LOGP: Cancel XO Core ON vote\n");
   if (vos_chipVoteXOCore(NULL, NULL, NULL, VOS_FALSE) != VOS_STATUS_SUCCESS)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR, "Could not cancel XO Core ON vote. Not returning failure."
                                         "Power consumed will be high\n");
   }

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing SME STOP",__func__);
   //Stop SME - Cannot invoke vos_stop as vos_stop relies
   //on threads being running to process the SYS Stop
   vosStatus = sme_Stop( pHddCtx->hHal, TRUE );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing MAC STOP",__func__);
   //Stop MAC (PE and HAL)
   vosStatus = macStop( pHddCtx->hHal, HAL_STOP_TYPE_SYS_RESET);
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing TL STOP",__func__);
   //Stop TL
   vosStatus = WLANTL_Stop( pVosContext );
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );

#ifdef ANI_BUS_TYPE_SDIO
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing BAL STOP",__func__);
   vosStatus = WLANBAL_Stop( pVosContext );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
         "%s: Failed to stop BAL",__func__);
      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   }
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
   hdd_unregister_mcast_bcast_filter(pHddCtx);
#endif

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Flush Queues",__func__);
   //Clean up message queues of TX and MC thread
   vos_sched_flush_mc_mqs(vosSchedContext);
   vos_sched_flush_tx_mqs(vosSchedContext);
#ifdef FEATURE_WLAN_INTEGRATED_SOC
   vos_sched_flush_rx_mqs(vosSchedContext);
#endif

   //Deinit all the TX and MC queues
   vos_sched_deinit_mqs(vosSchedContext);

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing VOS Close",__func__);

   //Close VOSS
   vos_close(pVosContext);

#ifdef ANI_BUS_TYPE_SDIO
   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing BAL Close",__func__);
   vosStatus = WLANBAL_Close(pVosContext);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, 
          "%s: Failed to close BAL",__func__);
      VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   }
   //Get the Current SDIO Func
   sdio_func_dev_current = libra_getsdio_funcdev();

   if(NULL != sdio_func_dev_current) {
      libra_detect_card_change();
      attempts = 0;
      do {
         msleep(100);
         //Get the SDIO func device
         sdio_func_dev_current = libra_getsdio_funcdev();
         if(NULL == sdio_func_dev_current) {
            hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Card Removed Successfully",__func__);
            break;
         }
         else {
            hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Failed to Remove the Card: Trying Again",__func__);
            attempts++;
         }
      } while (attempts < LIBRA_CARD_REMOVE_DETECT_MAX_COUNT);

      if(LIBRA_CARD_REMOVE_DETECT_MAX_COUNT == attempts) {
         hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Failed to Remove the Card: Fatal",__func__);
         goto err_fail;
      }
   }
#endif

#ifdef TIMER_MANAGER
   vos_timer_exit();
#endif

#ifdef MEMORY_DEBUG
   vos_mem_clean();
#endif
#ifdef ANI_BUS_TYPE_SDIO
   //Reinitialize the variable
   attempts = 0;
#endif

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Powering Up chip Again",__func__);
   //Power Up Libra WLAN card first if not already powered up
   vosStatus = vos_chipPowerUp(NULL,NULL,NULL);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Libra WLAN not Powered Up. "
             "exiting", __func__);
      goto err_pwr_fail;
   }

#ifdef ANI_BUS_TYPE_SDIO
   // Trigger card detect
   libra_detect_card_change();

   //Reinitialize the variable
   attempts = 0;

   do {
      msleep(500);

      //Get the SDIO func device
      sdio_func_dev_new = libra_getsdio_funcdev();
      if(sdio_func_dev_new != NULL)
      {
         //Not needed but in case it causes probs then put a loop and set for each adapter
         //SET_NETDEV_DEV(pAdapter->dev, &sdio_func_dev_new->dev); 
         libra_sdio_setprivdata (sdio_func_dev_new, pHddCtx);
         atomic_set(&pHddCtx->sdio_claim_count, 0);
         pHddCtx->parent_dev = &sdio_func_dev_new->dev;
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
          "%s: Card Detected Successfully %p",__func__, 
          sdio_func_dev_new);
         break;
      }
      else
      {
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
          "%s: Failed to detect card change %p",__func__, 
          sdio_func_dev_new);     
         attempts++;
      }
   }while (attempts < LIBRA_CARD_INSERT_DETECT_MAX_COUNT);

   if(LIBRA_CARD_INSERT_DETECT_MAX_COUNT == attempts){
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Libra WLAN fail to detect in reset, "
             "exiting", __func__);
      goto err_fail;
   }
   //Get the SDIO func device
   sdio_func_dev_new = libra_getsdio_funcdev();
   if(sdio_func_dev_new != NULL)
   {
       sd_claim_host(sdio_func_dev_new);
       /* Enable IRQ capabilities in host controller */
       libra_disable_sdio_irq_capability(sdio_func_dev_new, 0);
       libra_enable_sdio_irq(sdio_func_dev_new, 1);
       sd_release_host(sdio_func_dev_new);
   }
   else
   {
        /* Our card got removed before LOGP. */
        hddLog(VOS_TRACE_LEVEL_FATAL, "%s: sdio_func_dev is NULL!",__func__);
        goto err_fail;
   }

   vosStatus = WLANBAL_Open(pVosContext);
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
        "%s: Failed to open BAL",__func__);
     goto err_fail;
   }

   vosStatus = WLANSAL_Start(pVosContext);
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Failed to start SAL",__func__);
      goto err_balclose;
   }

   /* Chip is reset so allow Lower MAC to start accessing WLAN registers. Note HDD is still blocked */
   vos_set_logp_in_progress(VOS_MODULE_ID_HDD, FALSE);

   /* Start BAL */
   vosStatus = WLANBAL_Start(pVosContext);

   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to start BAL",__func__);
      goto err_salstop;
   }
#endif
   // Open VOSS 
   vosStatus = vos_open( &pVosContext, 0);

   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_open failed",__func__);
#ifdef ANI_BUS_TYPE_SDIO
      goto err_balstop;
#else
      goto err_pwr_fail;
#endif
   }

   /* Save the hal context in Adapter */
   pHddCtx->hHal = (tHalHandle)vos_get_context( VOS_MODULE_ID_SME, pVosContext );

   if ( NULL == pHddCtx->hHal )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HAL context is null",__func__);      
      goto err_vosclose;
   }
   // Set the SME configuration parameters...
   vosStatus = hdd_set_sme_config(pHddCtx);

   if ( VOS_STATUS_SUCCESS != vosStatus )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Failed hdd_set_sme_config",__func__); 
      goto err_vosclose;
   }

   //Initialize the WMM module
   vosStatus = hdd_wmm_init(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: hdd_wmm_init failed", __FUNCTION__);
      goto err_vosclose;
   }

   /*Start VOSS which starts up the SME/MAC/HAL modules and everything else
     Note: Firmware image will be read and downloaded inside vos_start API */
   vosStatus = vos_start( pVosContext );
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_start failed",__func__);
      goto err_vosclose;
   }

   vosStatus = hdd_post_voss_start_config( pHddCtx );
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hdd_post_voss_start_config failed", 
         __func__);
      goto err_vosstop;
   }

   /* Restart all adapters */
   hdd_start_all_adapters(pHddCtx);
   pHddCtx->isLogpInProgress = FALSE;
   pHddCtx->hdd_mcastbcast_filter_set = FALSE;
#ifdef CONFIG_HAS_EARLYSUSPEND
   hdd_register_mcast_bcast_filter(pHddCtx);
#endif

   // Allow the phone to go to sleep
   hdd_allow_suspend();

   goto success;

err_vosstop:
   vos_stop(pVosContext);

err_vosclose:
   vos_close(pVosContext ); 
   vos_sched_close(pVosContext);

#ifdef ANI_BUS_TYPE_SDIO
err_balstop:
#ifndef ANI_MANF_DIAG 
   wlan_hdd_enable_deepsleep(pVosContext);
#endif
   WLANBAL_Stop(pVosContext);
   WLANBAL_SuspendChip(pVosContext);

err_salstop:
   WLANSAL_Stop(pVosContext);

err_balclose:
   WLANBAL_Close(pVosContext);

err_fail:
   //Assert Deep sleep signal now to put Libra HW in lowest power state
   vosStatus = vos_chipAssertDeepSleep( NULL, NULL, NULL );

   //Vote off any PMIC voltage supplies
   vos_chipPowerDown(NULL, NULL, NULL);
#endif

err_pwr_fail:
   vos_chipVoteOffXOBuffer(NULL, NULL, NULL);

   // Allow the phone to go to sleep
   hdd_allow_suspend();

#ifdef CONFIG_HAS_EARLYSUSPEND
   // unregister suspend/resume callbacks
   if(pHddCtx->cfg_ini->nEnableSuspend)
      unregister_wlan_suspend();
#endif

   // Unregister the Net Device Notifier
   unregister_netdevice_notifier(&hdd_netdev_notifier);

  //Clean up HDD Nlink Service
   send_btc_nlink_msg(WLAN_MODULE_DOWN_IND, 0); 
   nl_srv_exit();

  hdd_close_all_adapters(pHddCtx);
  //Free up dynamically allocated members inside HDD Adapter
   kfree(pHddCtx->cfg_ini);
   pHddCtx->cfg_ini= NULL;

#ifdef CONFIG_CFG80211
   wiphy_unregister(pHddCtx->wiphy);
   wiphy_free(pHddCtx->wiphy);
#else
   vos_mem_free(pHddCtx);
#endif

#ifdef ANI_BUS_TYPE_SDIO
   WLANSAL_Close(pVosContext);
#endif
   vos_preClose(&pVosContext);

#ifdef MEMORY_DEBUG
   vos_mem_exit();
#endif

   return -1;

success:
   //Trigger replay of BTC events
   send_btc_nlink_msg(WLAN_MODULE_DOWN_IND, 0);
   return VOS_STATUS_SUCCESS;

}



VOS_STATUS hdd_wlan_reset_initialization(void) 
{
#ifdef ANI_BUS_TYPE_SDIO
   struct sdio_func *sdio_func_dev = NULL;
   v_U8_t  regValue = 0;
   int  err_ret = 0;
#endif
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

#ifdef ANI_BUS_TYPE_SDIO
   /* Clear pending interrupt and  disable Interrupts. Use only CMD52 */
   VOS_TRACE( VOS_MODULE_ID_SAL, VOS_TRACE_LEVEL_FATAL, 
          "%s LOGP in progress. Disabling Interrupt", __func__);

   sdio_func_dev = libra_getsdio_funcdev();

   if(sdio_func_dev == NULL)
   {
      /* Our card got removed before LOGP. Continue with reset anyways */
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: sdio_func_dev is NULL!",__func__);
      return VOS_STATUS_SUCCESS;
   }

   sd_claim_host(sdio_func_dev);

   regValue = 0;
   libra_sdiocmd52(sdio_func_dev, QWLAN_SIF_SIF_INT_EN_REG,  
                             &regValue, 1, &err_ret);

   VOS_TRACE( VOS_MODULE_ID_SAL, VOS_TRACE_LEVEL_FATAL, 
      "%s LOGP Cleared SIF_SIF_INT_EN_REG status:%d", __func__,err_ret);

   regValue = 0;
   libra_sdiocmd52(sdio_func_dev, QWLAN_SIF_BAR4_INT_PEND_REG,  
                             &regValue, 1, &err_ret);

   VOS_TRACE( VOS_MODULE_ID_SAL, VOS_TRACE_LEVEL_FATAL, 
      "%s LOGP Cleared SIF_BAR4_INT_PEND_REG status :%d", __func__,err_ret);

   regValue = 0;
   libra_sdiocmd52(sdio_func_dev, QWLAN_SIF_BAR4_INT_ENABLE_REG,  
                             &regValue, 1, &err_ret);

   VOS_TRACE( VOS_MODULE_ID_SAL, VOS_TRACE_LEVEL_FATAL, 
      "%s LOGP Cleared SIF_BAR4_INT_ENABLE_REG: Status:%d", __func__,err_ret);

   hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Doing Suspend Chip",__func__);

   //Put the chip is standby before asserting deep sleep
   WLANBAL_SuspendChip_NoLock( pVosContext );

   sd_release_host(sdio_func_dev);
#endif

   return VOS_STATUS_SUCCESS;
}


void register_wlan_suspend(void)
{
   hddLog(VOS_TRACE_LEVEL_INFO, "%s: Register WLAN suspend/resume "
            "callbacks",__func__);
   wlan_early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
   wlan_early_suspend.suspend = hdd_suspend_wlan;
   wlan_early_suspend.resume = hdd_resume_wlan;
   register_early_suspend(&wlan_early_suspend);
}

void unregister_wlan_suspend(void)
{
   hddLog(VOS_TRACE_LEVEL_INFO, "%s: Unregister WLAN suspend/resume "
            "callbacks",__func__);
   unregister_early_suspend(&wlan_early_suspend);
}
#endif

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
#ifdef CONFIG_HAS_EARLYSUSPEND
    /* unregister suspend/resume callbacks */
    if(pHddCtx->cfg_ini->nEnableSuspend)
    {
        unregister_wlan_suspend();
    }
#endif
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

#ifdef CONFIG_HAS_EARLYSUSPEND
   hdd_unregister_mcast_bcast_filter(pHddCtx);
#endif
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
#ifdef WLAN_BTAMP_FEATURE
   hdd_config_t     *pConfig = NULL;
   WLANBAP_ConfigType btAmpConfig;
#endif

   hdd_prevent_suspend();
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
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: hdd_wmm_init failed", __FUNCTION__);
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
#ifdef CONFIG_HAS_EARLYSUSPEND
   hdd_register_mcast_bcast_filter(pHddCtx);
#endif

   /* Register with platform driver as client for Suspend/Resume */
   vosStatus = hddRegisterPmOps(pHddCtx);
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: hddRegisterPmOps failed",__func__);
      goto err_bap_stop;
   }
#ifdef CONFIG_HAS_EARLYSUSPEND
   // Register suspend/resume callbacks
   if(pHddCtx->cfg_ini->nEnableSuspend)
   {
      register_wlan_suspend();
   }
#endif
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
#ifdef CONFIG_HAS_EARLYSUSPEND
   /* unregister suspend/resume callbacks */
   if (pHddCtx->cfg_ini->nEnableSuspend)
      unregister_wlan_suspend();
#endif
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

#ifdef CONFIG_CFG80211
       wiphy_unregister(pHddCtx->wiphy);
       wiphy_free(pHddCtx->wiphy);
#else
       vos_mem_free(pHddCtx);
#endif
   }
   vos_preClose(&pVosContext);

#ifdef MEMORY_DEBUG
   vos_mem_exit();
#endif

err_re_init:
   /* Allow the phone to go to sleep */
   hdd_allow_suspend();
   return -1;

success:
   /* Trigger replay of BTC events */
   send_btc_nlink_msg(WLAN_MODULE_DOWN_IND, 0);
   return VOS_STATUS_SUCCESS;
}
