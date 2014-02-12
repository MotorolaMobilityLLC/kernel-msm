/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined( BAP_HDD_MISC_H )
#define BAP_HDD_MISC_H

/**===========================================================================
  
  \file  BAP_HDD_MISC.h
  
  \brief Linux HDD Adapter Type
         Copyright 2008 (c) Qualcomm, Incorporated.
         All Rights Reserved.
         Qualcomm Confidential and Proprietary.
  
  ==========================================================================*/
  
/*--------------------------------------------------------------------------- 
  Include files
  -------------------------------------------------------------------------*/ 
  
#include <bapApi.h>
#include <vos_types.h>
/*--------------------------------------------------------------------------- 
  Function declarations and documenation
  -------------------------------------------------------------------------*/ 

/**---------------------------------------------------------------------------
  
  \brief WLANBAP_SetConfig() - To updates some configuration for BAP module in
  SME
  
  This should be called after WLANBAP_Start().
  
  \param  - NA
  
  \return -
      The result code associated with performing the operation  

    VOS_STATUS_E_FAILURE:  failed to set the config in SME BAP 
    VOS_STATUS_SUCCESS:  Success

              
  --------------------------------------------------------------------------*/
VOS_STATUS WLANBAP_SetConfig
(
    WLANBAP_ConfigType *pConfig
);

/**---------------------------------------------------------------------------
  
  \brief WLANBAP_RegisterWithHCI() - To register WLAN PAL with HCI
  
  
  \param
   pAdapter : HDD adapter
  
  \return -
      The result code associated with performing the operation  

    VOS_STATUS_E_FAILURE:  failed to register with HCI 
    VOS_STATUS_SUCCESS:  Success

              
  --------------------------------------------------------------------------*/
VOS_STATUS WLANBAP_RegisterWithHCI(hdd_adapter_t *pAdapter);

/**---------------------------------------------------------------------------
  
  \brief WLANBAP_DeregisterFromHCI() - To deregister WLAN PAL with HCI
  
  
  \param - NA
  
  \return -
      The result code associated with performing the operation  

    VOS_STATUS_E_FAILURE:  failed to deregister with HCI 
    VOS_STATUS_SUCCESS:  Success

              
  --------------------------------------------------------------------------*/
VOS_STATUS WLANBAP_DeregisterFromHCI(void);

/**---------------------------------------------------------------------------
  
  \brief WLANBAP_StopAmp() - To stop the current AMP traffic/connection
  
  
  \param - NA
  
  \return -
      The result code associated with performing the operation  

    VOS_STATUS_E_FAILURE:  failed to stop AMP connection 
    VOS_STATUS_SUCCESS:  Success

              
  --------------------------------------------------------------------------*/
VOS_STATUS WLANBAP_StopAmp(void);

/**---------------------------------------------------------------------------
  
  \brief WLANBAP_AmpSessionOn() - To check if AMP connection is on currently
  
  
  \param - NA
  
  \return -
      The result code associated with performing the operation  

    VOS_TRUE:  AMP connection is on 
    VOS_FALSE: AMP connection is not on

              
  --------------------------------------------------------------------------*/
v_BOOL_t WLANBAP_AmpSessionOn(void);
#endif    // end #if !defined( BAP_HDD_MISC_H )
