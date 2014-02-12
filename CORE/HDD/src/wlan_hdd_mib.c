/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#include "wlan_hdd_includes.h"


static inline v_VOID_t mibGetDot11DesiredBssType( hdd_adapter_t *pAdapter, eMib_dot11DesiredBssType *pDot11DesiredBssType )
{
    *pDot11DesiredBssType = pAdapter->hdd_mib.mibDot11DesiredBssType;
    return;
}

static inline VOS_STATUS mibSetDot11DesiredBssType( hdd_adapter_t *pAdapter, eMib_dot11DesiredBssType mibDot11DesiredBssType )
{
    pAdapter->hdd_mib.mibDot11DesiredBssType = mibDot11DesiredBssType;
    return( VOS_STATUS_SUCCESS );
}

v_BOOL_t mibIsDot11DesiredBssTypeInfrastructure( hdd_adapter_t *pAdapter )
{
    eMib_dot11DesiredBssType mibDot11DesiredBssType; 
    mibGetDot11DesiredBssType( pAdapter, &mibDot11DesiredBssType );
    
    return( eMib_dot11DesiredBssType_infrastructure == mibDot11DesiredBssType );   
}


static inline v_BOOL_t mibIsDot11DesiredBssTypeIndependent( hdd_adapter_t *pAdapter )
{
    eMib_dot11DesiredBssType mibDot11DesiredBssType; 
    mibGetDot11DesiredBssType( pAdapter, &mibDot11DesiredBssType );
    
    return( eMib_dot11DesiredBssType_independent == mibDot11DesiredBssType );   
}

static inline v_VOID_t mibGetDot11IbssJoinOnly( hdd_adapter_t *pAdapter, v_BOOL_t *pdot11IbssJoinOnly )
{
    *pdot11IbssJoinOnly = pAdapter->hdd_mib.dot11IbssJoinOnly;
    return;
}

static inline VOS_STATUS mibSetDot11IbssJoinOnly( hdd_adapter_t *pAdapter, v_BOOL_t dot11IbssJoinOnly )
{
    pAdapter->hdd_mib.dot11IbssJoinOnly = dot11IbssJoinOnly;
    return( VOS_STATUS_SUCCESS );
}

static inline VOS_STATUS mibSetDot11NICPowerState( hdd_adapter_t *pAdapter, eMib_dot11NICPowerState *pMibDot11NICPowerState )
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;

    pAdapter->hdd_mib.mibDot11NICPowerState = *pMibDot11NICPowerState;
    
    return( status );
}

static inline VOS_STATUS mibSetDot11NICPowerStateOff( hdd_adapter_t *pAdapter )
{
    eMib_dot11NICPowerState dot11NICPowerState = eMib_dot11NICPowerState_OFF;
    return( mibSetDot11NICPowerState( pAdapter, &dot11NICPowerState ) );
}

static inline void mibGetDot11NICPowerState( hdd_adapter_t *pAdapter, eMib_dot11NICPowerState *pMibDot11NICPowerState )
{
    *pMibDot11NICPowerState = pAdapter->hdd_mib.mibDot11NICPowerState;
    
    return;
}

static inline v_BOOL_t mibIsDot11NICPowerStateOn( hdd_adapter_t *pAdapter )
{
    eMib_dot11NICPowerState dot11NICPowerState;

    mibGetDot11NICPowerState( pAdapter, &dot11NICPowerState );
    
    return( eMib_dot11NICPowerState_ON == dot11NICPowerState );
}

static inline v_BOOL_t mibIsDot11NICPowerStateOff( hdd_adapter_t *pAdapter )
{
    return( !mibIsDot11NICPowerStateOn( pAdapter ) );
}

static inline VOS_STATUS mibSetDot11DesiredSsidList( hdd_adapter_t *pAdapter, sMib_dot11DesiredSsidList *pDot11DesiredSsidList )
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
        
    if ( pDot11DesiredSsidList->cEntries > MIB_DOT11_DESIRED_SSID_LIST_MAX_COUNT )

        pAdapter->hdd_mib.mibDot11DesiredSsidList = *pDot11DesiredSsidList ;
              
    return( status );
}

static inline VOS_STATUS mibSetDot11DesiredBssidList( hdd_adapter_t *pAdapter, sMib_dot11DesiredBssidList *pDot11DesiredBssidList )
{   
    pAdapter->hdd_mib.mibDot11DesiredBssidList = *pDot11DesiredBssidList;
    
    return( VOS_STATUS_SUCCESS );
}


static inline v_VOID_t mibGetDot11DesiredBssidList( hdd_adapter_t *pAdapter, sMib_dot11DesiredBssidList *pMibDot11DesiredBssidList )
{
    *pMibDot11DesiredBssidList = pAdapter->hdd_mib.mibDot11DesiredBssidList;
    
    return;
}


static inline v_VOID_t mibGetDot11DesiredSsidList( hdd_adapter_t *pAdapter, sMib_dot11DesiredSsidList *pMibDot11DesiredSsidList )
{
    *pMibDot11DesiredSsidList = pAdapter->hdd_mib.mibDot11DesiredSsidList;
    
    return;
}


static inline VOS_STATUS mibSetDot11AutoConfigEnabled( hdd_adapter_t *pAdapter, eMib_dot11AutoConfigEnabled *pMibDot11AutoConfigEnabled )
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;

    pAdapter->hdd_mib.mibDot11AutoConfigEnabled = *pMibDot11AutoConfigEnabled;
    
    return( status );
}


static inline v_VOID_t mibGetDot11AutoConfigEnabled( hdd_adapter_t *pAdapter, eMib_dot11AutoConfigEnabled *pMibDot11AutoConfigEnabled )
{
    *pMibDot11AutoConfigEnabled = pAdapter->hdd_mib.mibDot11AutoConfigEnabled;
    
    return;
}

static inline VOS_STATUS mibSetDot11MacExcludeList( hdd_adapter_t *pAdapter, sMib_dot11MacExcludeList *pDot11MacExcludeList )
{
    pAdapter->hdd_mib.mibDot11MacExcludeList = *pDot11MacExcludeList;
    
    return( VOS_STATUS_SUCCESS );
}

static inline VOS_STATUS mibGetDot11MacExcludeList( hdd_adapter_t *pAdapter, sMib_dot11MacExcludeList *pDot11MacExcludeList )
{
    *pDot11MacExcludeList =  pAdapter->hdd_mib.mibDot11MacExcludeList;

    return( VOS_STATUS_SUCCESS );
}

static inline void mibSetDefaultDot11MacExcludeList( hdd_adapter_t *pAdapter )
{
    pAdapter->hdd_mib.mibDot11MacExcludeList.cEntries = 0;
}

static inline VOS_STATUS mibSetDot11HardwarePHYState( hdd_adapter_t *pAdapter, eMib_dot11HardwarePHYState *pMibDot11HardwarePHYState )
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    
    pAdapter->hdd_mib.mibDot11HardwarePHYState = *pMibDot11HardwarePHYState;
    
    return( status );
}


static inline void mibGetDot11HardwarePHYState( hdd_adapter_t *pAdapter, eMib_dot11HardwarePHYState *pMibDot11HardwarePHYState )
{
    *pMibDot11HardwarePHYState = pAdapter->hdd_mib.mibDot11HardwarePHYState;
    
    return;
}

static inline void mibSetDefaultDot11PrivacyExemptionList( hdd_adapter_t *pAdapter )
{
    pAdapter->hdd_mib.mibDot11PrivacyExemptionList.cEntries = 0;
}


static inline void mibGetDot11PowerSavingLevel( hdd_adapter_t *pAdapter, eMib_dot11PowerSavingLevel *pMibDot11PowerSavingLevel )
{
    *pMibDot11PowerSavingLevel = pAdapter->hdd_mib.mibDot11PowerSavingLevel;
    
    return;
}


static inline void mibGetDevicePowerState( hdd_adapter_t *pAdapter, eMib_DevicePowerState *pMibDevicePowerState )
{
    *pMibDevicePowerState = pAdapter->hdd_mib.mibDevicePowerState;

    return;
}


