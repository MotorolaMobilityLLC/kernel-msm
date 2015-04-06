/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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

/**=========================================================================

                       EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$   $DateTime: $ $Author: $


  when        who    what, where, why
  --------    ---    --------------------------------------------------------
  03/29/11    tbh    Created module.

  ==========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include <wlan_hdd_dev_pwr.h>
#include <wcnss_api.h>

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *  Type Declarations
 * -------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------
 * Global variables.
 *-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------
 * Local variables.
 *-------------------------------------------------------------------------*/
/* Reference VoIP, 100msec delay make disconnect.
 * So TX sleep must be less than 100msec
 * Every 20msec TX frame will goes out.
 * 10 frame means 2seconds TX operation */
static const hdd_tmLevelAction_t thermalMigrationAction[WLAN_HDD_TM_LEVEL_MAX] =
{
   /* TM Level 0, Do nothing, just normal operation */
   {1, 0, 0, 0, 0xFFFFF},
   /* Tm Level 1, disable TX AMPDU */
   {0, 0, 0, 0, 0xFFFFF},
   /* TM Level 2, disable AMDPU,
    * TX sleep 100msec if TX frame count is larger than 16 during 300msec */
   {0, 0, 100, 300, 16},
   /* TM Level 3, disable AMDPU,
    * TX sleep 500msec if TX frame count is larger than 11 during 500msec */
   {0, 0, 500, 500, 11},
   /* TM Level 4, MAX TM level, enter IMPS */
   {0, 1, 1000, 500, 10}
};
#ifdef HAVE_WCNSS_SUSPEND_RESUME_NOTIFY
static bool suspend_notify_sent;
#endif


/*----------------------------------------------------------------------------

   @brief TX frame block timeout handler
          Resume TX, and reset TX frame count

   @param hdd_context_t pHddCtx
        Global hdd context

   @return NONE

----------------------------------------------------------------------------*/
void hddDevTmTxBlockTimeoutHandler(void *usrData)
{
   hdd_context_t        *pHddCtx = (hdd_context_t *)usrData;
   hdd_adapter_t        *staAdapater;
   /* Sanity, This should not happen */
   if(NULL == pHddCtx)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_ERROR,
                "%s: NULL Context", __func__);
      VOS_ASSERT(0);
      return;
   }

   staAdapater = hdd_get_adapter(pHddCtx, WLAN_HDD_INFRA_STATION);

   if(NULL == staAdapater)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_ERROR,
                "%s: NULL Adapter", __func__);
      VOS_ASSERT(0);
      return;
   }

   if(mutex_lock_interruptible(&pHddCtx->tmInfo.tmOperationLock))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_ERROR,
                "%s: Acquire lock fail", __func__);
      return;
   }
   pHddCtx->tmInfo.txFrameCount = 0;

   /* Resume TX flow */

   netif_tx_wake_all_queues(staAdapater->dev);
   pHddCtx->tmInfo.qBlocked = VOS_FALSE;
   mutex_unlock(&pHddCtx->tmInfo.tmOperationLock);

   return;
}

/*----------------------------------------------------------------------------

   @brief Register function
        Register Thermal Mitigation Level Changed handle callback function

   @param hdd_context_t pHddCtx
        Global hdd context

   @return General status code
        VOS_STATUS_SUCCESS       Registration Success
        VOS_STATUS_E_FAILURE     Registration Fail

----------------------------------------------------------------------------*/
VOS_STATUS hddDevTmRegisterNotifyCallback(hdd_context_t *pHddCtx)
{
   VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_INFO,
             "%s: Register TM Handler", __func__);

   wcnss_register_thermal_mitigation(pHddCtx->parent_dev ,hddDevTmLevelChangedHandler);

   /* Set Default TM Level as Lowest, do nothing */
   pHddCtx->tmInfo.currentTmLevel = WLAN_HDD_TM_LEVEL_0;
   vos_mem_zero(&pHddCtx->tmInfo.tmAction, sizeof(hdd_tmLevelAction_t));
   vos_timer_init(&pHddCtx->tmInfo.txSleepTimer,
                  VOS_TIMER_TYPE_SW,
                  hddDevTmTxBlockTimeoutHandler,
                  (void *)pHddCtx);
   mutex_init(&pHddCtx->tmInfo.tmOperationLock);
   pHddCtx->tmInfo.txFrameCount = 0;
   pHddCtx->tmInfo.blockedQueue = NULL;
   pHddCtx->tmInfo.qBlocked     = VOS_FALSE;
   return VOS_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------

   @brief Un-Register function
        Un-Register Thermal Mitigation Level Changed handle callback function

   @param hdd_context_t pHddCtx
        Global hdd context

   @return General status code
        VOS_STATUS_SUCCESS       Un-Registration Success
        VOS_STATUS_E_FAILURE     Un-Registration Fail

----------------------------------------------------------------------------*/
VOS_STATUS hddDevTmUnregisterNotifyCallback(hdd_context_t *pHddCtx)
{
   VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;

   wcnss_unregister_thermal_mitigation(hddDevTmLevelChangedHandler);

   if(VOS_TIMER_STATE_RUNNING ==
           vos_timer_getCurrentState(&pHddCtx->tmInfo.txSleepTimer))
   {
       vosStatus = vos_timer_stop(&pHddCtx->tmInfo.txSleepTimer);
       if(!VOS_IS_STATUS_SUCCESS(vosStatus))
       {
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                                "%s: Timer stop fail", __func__);
       }
   }

   // Destroy the vos timer...
   vosStatus = vos_timer_destroy(&pHddCtx->tmInfo.txSleepTimer);
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
       VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_ERROR,
                            "%s: Fail to destroy timer", __func__);
   }

   return VOS_STATUS_SUCCESS;
}
