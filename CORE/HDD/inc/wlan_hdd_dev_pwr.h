/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef __WLAN_HDD_DEV_PWR_H
#define __WLAN_HDD_DEV_PWR_H

#include <wlan_hdd_includes.h>
#include <wlan_hdd_power.h>
#include <vos_sched.h>
#include <vos_api.h>

/*----------------------------------------------------------------------------

   @brief Registration function.
        Register suspend, resume callback functions with platform driver. 

   @param hdd_context_t pHddCtx
        Global hdd context

   @return General status code
        VOS_STATUS_SUCCESS       Registration Success
        VOS_STATUS_E_FAILURE     Registration Fail

----------------------------------------------------------------------------*/
VOS_STATUS hddRegisterPmOps(hdd_context_t *pHddCtx);

/*----------------------------------------------------------------------------

   @brief De-registration function.
        Deregister the suspend, resume callback functions with platform driver

   @param hdd_context_t pHddCtx
        Global hdd context

   @return General status code
        VOS_STATUS_SUCCESS       De-Registration Success
        VOS_STATUS_E_FAILURE     De-Registration Fail

----------------------------------------------------------------------------*/
VOS_STATUS hddDeregisterPmOps(hdd_context_t *pHddCtx);

/*----------------------------------------------------------------------------

   @brief TM Level Change handler
          Received Tm Level changed notification

   @param dev : Device context
          changedTmLevel : Changed new TM level

   @return 

----------------------------------------------------------------------------*/
void hddDevTmLevelChangedHandler(struct device *dev, int changedTmLevel);

/*----------------------------------------------------------------------------

   @brief Register function
        Register Thermal Mitigation Level Changed handle callback function

   @param hdd_context_t pHddCtx
        Global hdd context

   @return General status code
        VOS_STATUS_SUCCESS       Registration Success
        VOS_STATUS_E_FAILURE     Registration Fail

----------------------------------------------------------------------------*/
VOS_STATUS hddDevTmRegisterNotifyCallback(hdd_context_t *pHddCtx);

/*----------------------------------------------------------------------------

   @brief Un-Register function
        Un-Register Thermal Mitigation Level Changed handle callback function

   @param hdd_context_t pHddCtx
        Global hdd context

   @return General status code
        VOS_STATUS_SUCCESS       Un-Registration Success
        VOS_STATUS_E_FAILURE     Un-Registration Fail

----------------------------------------------------------------------------*/
VOS_STATUS hddDevTmUnregisterNotifyCallback(hdd_context_t *pHddCtx);

#endif
