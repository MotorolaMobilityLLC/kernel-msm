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

/*===========================================================================

                      s a p M o d u l e . C

  OVERVIEW:

  This software unit holds the implementation of the WLAN SAP modules
  functions providing EXTERNAL APIs. It is also where the global SAP module
  context gets initialised

  DEPENDENCIES:

  Are listed for each API below.
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.



  when               who                 what, where, why
----------       ---                --------------------------------------------------------
03/15/10     SOFTAP team            Created module
06/03/10     js                     Added support to hostapd driven
 *                                  deauth/disassoc/mic failure

===========================================================================*/

/* $Header$ */

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "wlan_qct_tl.h"
#include "vos_trace.h"

// Pick up the sme callback registration API
#include "sme_Api.h"

// SAP API header file

#include "sapInternal.h"
#include "smeInside.h"
#include "regdomain_common.h"
#include "_ieee80211_common.h"

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
#define SAP_DEBUG

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *  External declarations for global context
 * -------------------------------------------------------------------------*/
//  No!  Get this from VOS.
//  The main per-Physical Link (per WLAN association) context.
ptSapContext  gpSapCtx;

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Function Declarations and Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Externalized Function Definitions
* -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Function Declarations and Documentation
 * -------------------------------------------------------------------------*/

/*==========================================================================
  FUNCTION    WLANSAP_Open

  DESCRIPTION
    Called at driver initialization (vos_open). SAP will initialize
    all its internal resources and will wait for the call to start to
    register with the other modules.

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx    : Pointer to the global vos context; a handle to SAP's

  RETURN VALUE
    The result code associated with performing the operation

#ifdef WLAN_FEATURE_MBSSID
    v_PVOID_t   : Pointer to the SAP context
#else
    VOS_STATUS_E_FAULT: Pointer to SAP cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS: Success
#endif

  SIDE EFFECTS
============================================================================*/
#ifdef WLAN_FEATURE_MBSSID
v_PVOID_t
#else
VOS_STATUS
#endif
WLANSAP_Open
(
    v_PVOID_t  pvosGCtx
)
{
    ptSapContext pSapCtx = NULL;

    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
#ifdef WLAN_FEATURE_MBSSID
    // amically allocate the sapContext
    pSapCtx = (ptSapContext)vos_mem_malloc(sizeof(tSapContext));
#else
    if (NULL == pvosGCtx)
    {
       VOS_ASSERT(pvosGCtx);
       return VOS_STATUS_E_FAULT;
    }
    /*------------------------------------------------------------------------
         Allocate (and sanity check?!) SAP control block
       ------------------------------------------------------------------------*/
    vos_alloc_context(pvosGCtx, VOS_MODULE_ID_SAP, (v_VOID_t **)&pSapCtx, sizeof(tSapContext));
#endif

    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);
#ifdef WLAN_FEATURE_MBSSID
        return NULL;
#else
        return VOS_STATUS_E_FAULT;
#endif
    }

    /*------------------------------------------------------------------------
        Clean up SAP control block, initialize all values
       ------------------------------------------------------------------------*/
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANSAP_Open");

    WLANSAP_CleanCB(pSapCtx, 0 /*do not empty*/);

    // Setup the "link back" to the VOSS context
    pSapCtx->pvosGCtx = pvosGCtx;

    // Store a pointer to the SAP context provided by VOSS
    gpSapCtx = pSapCtx;

    /*------------------------------------------------------------------------
        Allocate internal resources
       ------------------------------------------------------------------------*/

#ifdef WLAN_FEATURE_MBSSID
    return pSapCtx;
#else
    return VOS_STATUS_SUCCESS;
#endif
}// WLANSAP_Open

/*==========================================================================
  FUNCTION    WLANSAP_Start

  DESCRIPTION
    Called as part of the overall start procedure (vos_start). SAP will
    use this call to register with TL as the SAP entity for
    SAP RSN frames.

  DEPENDENCIES

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Pointer to SAP cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/

VOS_STATUS
WLANSAP_Start
(
    v_PVOID_t pCtx
)
{
    ptSapContext pSapCtx = NULL;

    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                 "WLANSAP_Start invoked successfully");
    /*------------------------------------------------------------------------
        Sanity check
        Extract SAP control block
    ------------------------------------------------------------------------*/
    pSapCtx = VOS_GET_SAP_CB(pCtx);

    if ( NULL == pSapCtx )
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /*------------------------------------------------------------------------
        For now, presume security is not enabled.
    -----------------------------------------------------------------------*/
    pSapCtx->ucSecEnabled = WLANSAP_SECURITY_ENABLED_STATE;


    /*------------------------------------------------------------------------
        Now configure the roaming profile links. To SSID and bssid.
    ------------------------------------------------------------------------*/
    // We have room for two SSIDs.
    pSapCtx->csrRoamProfile.SSIDs.numOfSSIDs = 1; // This is true for now.
    pSapCtx->csrRoamProfile.SSIDs.SSIDList = pSapCtx->SSIDList;  //Array of two
    pSapCtx->csrRoamProfile.SSIDs.SSIDList[0].SSID.length = 0;
    pSapCtx->csrRoamProfile.SSIDs.SSIDList[0].handoffPermitted = VOS_FALSE;
    pSapCtx->csrRoamProfile.SSIDs.SSIDList[0].ssidHidden = pSapCtx->SSIDList[0].ssidHidden;

    pSapCtx->csrRoamProfile.BSSIDs.numOfBSSIDs = 1; // This is true for now.
    pSapCtx->csrRoamProfile.BSSIDs.bssid = &pSapCtx->bssid;

    // Now configure the auth type in the roaming profile. To open.
    pSapCtx->csrRoamProfile.negotiatedAuthType = eCSR_AUTH_TYPE_OPEN_SYSTEM; // open is the default

    if( !VOS_IS_STATUS_SUCCESS( vos_lock_init( &pSapCtx->SapGlobalLock)))
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                 "WLANSAP_Start failed init lock");
        return VOS_STATUS_E_FAULT;
    }

    return VOS_STATUS_SUCCESS;
}/* WLANSAP_Start */

/*==========================================================================

  FUNCTION    WLANSAP_Stop

  DESCRIPTION
    Called by vos_stop to stop operation in SAP, before close. SAP will suspend all
    BT-AMP Protocol Adaption Layer operation and will wait for the close
    request to clean up its resources.

  DEPENDENCIES

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Pointer to SAP cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_Stop
(
    v_PVOID_t pCtx
)
{
    ptSapContext pSapCtx = NULL;

    /*------------------------------------------------------------------------
        Sanity check
        Extract SAP control block
    ------------------------------------------------------------------------*/
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                "WLANSAP_Stop invoked successfully ");

    pSapCtx = VOS_GET_SAP_CB(pCtx);
    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    sapFreeRoamProfile(&pSapCtx->csrRoamProfile);

    if( !VOS_IS_STATUS_SUCCESS( vos_lock_destroy( &pSapCtx->SapGlobalLock ) ) )
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                 "WLANSAP_Stop failed destroy lock");
        return VOS_STATUS_E_FAULT;
    }
    /*------------------------------------------------------------------------
        Stop SAP (de-register RSN handler!?)
    ------------------------------------------------------------------------*/

    return VOS_STATUS_SUCCESS;
}/* WLANSAP_Stop */

/*==========================================================================
  FUNCTION    WLANSAP_Close

  DESCRIPTION
    Called by vos_close during general driver close procedure. SAP will clean up
    all the internal resources.

  DEPENDENCIES

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Pointer to SAP cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_Close
(
    v_PVOID_t pCtx
)
{
    ptSapContext pSapCtx = NULL;

    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    /*------------------------------------------------------------------------
        Sanity check
        Extract SAP control block
    ------------------------------------------------------------------------*/
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                 "WLANSAP_Close invoked");


    pSapCtx = VOS_GET_SAP_CB(pCtx);
    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /*------------------------------------------------------------------------
        Cleanup SAP control block.
    ------------------------------------------------------------------------*/
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANSAP_Close");
#ifdef WLAN_FEATURE_MBSSID
    sapCleanupChannelList(pCtx);
#else
    sapCleanupChannelList();
#endif

    WLANSAP_CleanCB(pSapCtx, VOS_TRUE /* empty queues/lists/pkts if any*/);

#ifdef WLAN_FEATURE_MBSSID
    vos_mem_free(pSapCtx);
#else
    /*------------------------------------------------------------------------
        Free SAP context from VOSS global
    ------------------------------------------------------------------------*/
    vos_free_context(pCtx, VOS_MODULE_ID_SAP, pSapCtx);
#endif

    return VOS_STATUS_SUCCESS;
}/* WLANSAP_Close */

/*----------------------------------------------------------------------------
 * Utility Function implementations
 * -------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANSAP_CleanCB

  DESCRIPTION
    Clear out all fields in the SAP context.

  DEPENDENCIES

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Pointer to SAP cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_CleanCB
(
    ptSapContext pSapCtx,
    v_U32_t freeFlag // 0 /*do not empty*/);
)
{
    /*------------------------------------------------------------------------
        Sanity check SAP control block
    ------------------------------------------------------------------------*/

    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /*------------------------------------------------------------------------
        Clean up SAP control block, initialize all values
    ------------------------------------------------------------------------*/
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANSAP_CleanCB");

    vos_mem_zero( pSapCtx, sizeof(tSapContext));

    pSapCtx->pvosGCtx = NULL;

    pSapCtx->sapsMachine= eSAP_DISCONNECTED;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: Initializing State: %d, sapContext value = %p",
            __func__, pSapCtx->sapsMachine, pSapCtx);
    pSapCtx->sessionId = 0;
    pSapCtx->channel = 0;
    pSapCtx->isSapSessionOpen  = eSAP_FALSE;

    return VOS_STATUS_SUCCESS;
}// WLANSAP_CleanCB

/*==========================================================================
  FUNCTION    WLANSAP_pmcFullPwrReqCB

  DESCRIPTION
    Callback provide to PMC in the pmcRequestFullPower API.

  DEPENDENCIES

  PARAMETERS

    IN
    callbackContext:  The user passed in a context to identify
    status:           The halStatus

  RETURN VALUE
    None

  SIDE EFFECTS
============================================================================*/
void
WLANSAP_pmcFullPwrReqCB
(
    void *callbackContext,
    eHalStatus status
)
{
    if(HAL_STATUS_SUCCESS(status))
    {
         //If success what else to be handled???
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_FATAL,
               "WLANSAP_pmcFullPwrReqCB: PMC failed to put the chip in Full power");

    }

}// WLANSAP_pmcFullPwrReqCB

/*==========================================================================
  FUNCTION    WLANSAP_getState

  DESCRIPTION
    This api returns the current SAP state to the caller.

  DEPENDENCIES

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs

  RETURN VALUE
    Returns the SAP FSM state.
============================================================================*/

v_U8_t WLANSAP_getState
(
    v_PVOID_t pCtx
)
{
    ptSapContext pSapCtx = NULL;

    pSapCtx = VOS_GET_SAP_CB(pCtx);

    if ( NULL == pSapCtx )
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "%s: Invalid SAP pointer from pCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }
    return pSapCtx->sapsMachine;
}

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
/*==========================================================================
  FUNCTION    WLANSAP_CheckCCIntf

  DESCRIPTION Restart SAP if Concurrent Channel interfering

  DEPENDENCIES NA.

  PARAMETERS
  IN
  Ctx: Pointer to vos Context or Sap Context based on MBSSID

  RETURN VALUE NONE

  SIDE EFFECTS
============================================================================*/
v_U16_t WLANSAP_CheckCCIntf(v_PVOID_t Ctx)
{
    tHalHandle hHal;
    v_U16_t intf_ch;
    ptSapContext pSapCtx = VOS_GET_SAP_CB(Ctx);

    hHal = (tHalHandle)VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
    if (NULL == hHal)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid MAC context from pvosGCtx", __func__);
        return 0;
    }
    intf_ch = sme_CheckConcurrentChannelOverlap(hHal, 0, 0,
                                                pSapCtx->cc_switch_mode);
    return intf_ch;
}
#endif

/*==========================================================================
  FUNCTION   wlan_sap_get_vht_ch_width

  DESCRIPTION Returns the SAP VHT channel width.

  DEPENDENCIES NA.

  PARAMETERS
  IN
  ctx: Pointer to vos Context or Sap Context based on MBSSID

  RETURN VALUE VHT channnel width

  SIDE EFFECTS
============================================================================*/
v_U32_t wlan_sap_get_vht_ch_width(v_PVOID_t ctx) {
    ptSapContext sap_ctx = VOS_GET_SAP_CB(ctx);

    if (!sap_ctx) {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from ctx", __func__);
        return 0;
    }

    return sap_ctx->vht_channel_width;
}

/*==========================================================================
  FUNCTION   wlan_sap_set_vht_ch_width

  DESCRIPTION Sets the SAP VHT channel width.

  DEPENDENCIES NA.

  PARAMETERS
  IN
  ctx: Pointer to vos Context or Sap Context based on MBSSID
  vht_channel_width - VHT channel width

  RETURN VALUE NONE

  SIDE EFFECTS
============================================================================*/
void wlan_sap_set_vht_ch_width(v_PVOID_t ctx, v_U32_t vht_channel_width) {
    ptSapContext sap_ctx = VOS_GET_SAP_CB(ctx);

    if (!sap_ctx) {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from ctx", __func__);
        return;
    }

    sap_ctx->vht_channel_width = vht_channel_width;
}

/*==========================================================================
  FUNCTION    WLANSAP_SetScanAcsChannelParams

  DESCRIPTION
    This api function is used to copy Scan and Channel parameters from sap
    config to sap context.

  DEPENDENCIES

  PARAMETERS

    IN
    pConfig    : Pointer to the SAP config
    sapContext : Pointer to the SAP Context.
    pUsrContext: Parameter that will be passed
                 back in all the SAP callback events.

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Pointer to SAP cb is NULL ; access would cause a page
                        fault
    VOS_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_SetScanAcsChannelParams(tsap_Config_t *pConfig,
                                ptSapContext pSapCtx,
                                v_PVOID_t  pUsrContext)
{
    tHalHandle hHal = NULL;
    tANI_BOOLEAN restartNeeded;

    if (NULL == pConfig)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid pConfig passed ", __func__);
        return VOS_STATUS_E_FAULT;
    }

    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid pConfig passed ", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /* Channel selection is auto or configured */
    pSapCtx->channel = pConfig->channel;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
    pSapCtx->cc_switch_mode = pConfig->cc_switch_mode;
#endif
    pSapCtx->scanBandPreference = pConfig->scanBandPreference;
    pSapCtx->acsBandSwitchThreshold = pConfig->acsBandSwitchThreshold;
    pSapCtx->pUsrContext = pUsrContext;
    pSapCtx->enableOverLapCh = pConfig->enOverLapCh;
    /*
     * Set the BSSID to your "self MAC Addr" read
     * the mac address from Configuation ITEM received
     * from HDD
     */
    pSapCtx->csrRoamProfile.BSSIDs.numOfBSSIDs = 1;
    vos_mem_copy(pSapCtx->csrRoamProfile.BSSIDs.bssid,
                 pSapCtx->self_mac_addr,
                 sizeof( tCsrBssid ));

    /*
     * Save a copy to SAP context
     */
    vos_mem_copy(pSapCtx->csrRoamProfile.BSSIDs.bssid,
                 pConfig->self_macaddr.bytes, sizeof(v_MACADDR_t));
    vos_mem_copy(pSapCtx->self_mac_addr,
                 pConfig->self_macaddr.bytes, sizeof(v_MACADDR_t));

    hHal = (tHalHandle)VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
    if (NULL == hHal)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "%s: Invalid MAC context from pvosGCtx", __func__);
    }
    else
    {
        //If concurrent session is running that is already associated
        //then we just follow that sessions country info (whether
        //present or not doesn't maater as we have to follow whatever
        //STA session does)
        if ((0 == sme_GetConcurrentOperationChannel(hHal)) &&
             pConfig->ieee80211d)
        {
            /* Setting the region/country  information */
            sme_setRegInfo(hHal, pConfig->countryCode);
            sme_ResetCountryCodeInformation(hHal, &restartNeeded);
        }
    }

    return VOS_STATUS_SUCCESS;
}

/*==========================================================================
  FUNCTION    WLANSAP_StartBss

  DESCRIPTION
    This api function provides SAP FSM event eWLAN_SAP_PHYSICAL_LINK_CREATE for
    starting AP BSS

  DEPENDENCIES

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs
    pQctCommitConfig    : Pointer to configuration structure passed down from HDD(HostApd for Android)
    hdd_SapEventCallback: Callback function in HDD called by SAP to inform HDD about SAP results
    pUsrContext         : Parameter that will be passed back in all the SAP callback events.

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Pointer to SAP cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_StartBss
(
    v_PVOID_t  pCtx,//pwextCtx
    tpWLAN_SAPEventCB pSapEventCallback,
    tsap_Config_t *pConfig,
    v_PVOID_t  pUsrContext
)
{
    tWLAN_SAPEvent sapEvent;    /* State machine event*/
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    ptSapContext pSapCtx = NULL;
    tANI_BOOLEAN restartNeeded;
    tHalHandle hHal;
    tpAniSirGlobal pmac = NULL;

    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    /*------------------------------------------------------------------------
        Sanity check
        Extract SAP control block
    ------------------------------------------------------------------------*/
    if (VOS_STA_SAP_MODE == vos_get_conparam ())
    {
        pSapCtx = VOS_GET_SAP_CB(pCtx);

        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                     "WLANSAP_StartBss: sapContext=%p", pSapCtx);

        if ( NULL == pSapCtx )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                       "%s: Invalid SAP pointer from pCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        pSapCtx->sapsMachine = eSAP_DISCONNECTED;

        /* Channel selection is auto or configured */
        pSapCtx->channel = pConfig->channel;
        pSapCtx->vht_channel_width = pConfig->vht_channel_width;
        pSapCtx->ch_width_orig = pConfig->ch_width_orig;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
        pSapCtx->cc_switch_mode = pConfig->cc_switch_mode;
#endif
        pSapCtx->scanBandPreference = pConfig->scanBandPreference;
        pSapCtx->acsBandSwitchThreshold = pConfig->acsBandSwitchThreshold;
        pSapCtx->pUsrContext = pUsrContext;
        pSapCtx->enableOverLapCh = pConfig->enOverLapCh;
        pSapCtx->acs_cfg = &pConfig->acs_cfg;
	pSapCtx->secondary_ch = pConfig->sec_ch;

        //Set the BSSID to your "self MAC Addr" read the mac address from Configuation ITEM received from HDD
        pSapCtx->csrRoamProfile.BSSIDs.numOfBSSIDs = 1;

        //Save a copy to SAP context
        vos_mem_copy(pSapCtx->csrRoamProfile.BSSIDs.bssid,
                    pConfig->self_macaddr.bytes, sizeof(v_MACADDR_t));
        vos_mem_copy(pSapCtx->self_mac_addr,
                    pConfig->self_macaddr.bytes, sizeof(v_MACADDR_t));

        //copy the configuration items to csrProfile
        sapconvertToCsrProfile( pConfig, eCSR_BSS_TYPE_INFRA_AP, &pSapCtx->csrRoamProfile);
        hHal = (tHalHandle)VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if (NULL == hHal)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                       "%s: Invalid MAC context from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        else
        {
            //If concurrent session is running that is already associated
            //then we just follow that sessions country info (whether
            //present or not doesn't maater as we have to follow whatever
            //STA session does)
            if ((0 == sme_GetConcurrentOperationChannel(hHal)) &&
                pConfig->ieee80211d)
            {
                /* Setting the region/country  information */
                sme_setRegInfo(hHal, pConfig->countryCode);
                sme_ResetCountryCodeInformation(hHal, &restartNeeded);
            }
        }

        pmac = PMAC_STRUCT( hHal );
        /*
         * Copy the DFS Test Mode setting to pmac for
         * access in lower layers
         */
        pmac->sap.SapDfsInfo.disable_dfs_ch_switch =
                                   pConfig->disableDFSChSwitch;
        // Copy MAC filtering settings to sap context
        pSapCtx->eSapMacAddrAclMode = pConfig->SapMacaddr_acl;
        vos_mem_copy(pSapCtx->acceptMacList, pConfig->accept_mac, sizeof(pConfig->accept_mac));
        pSapCtx->nAcceptMac = pConfig->num_accept_mac;
        sapSortMacList(pSapCtx->acceptMacList, pSapCtx->nAcceptMac);
        vos_mem_copy(pSapCtx->denyMacList, pConfig->deny_mac, sizeof(pConfig->deny_mac));
        pSapCtx->nDenyMac = pConfig->num_deny_mac;
        sapSortMacList(pSapCtx->denyMacList, pSapCtx->nDenyMac);
        /* Fill in the event structure for FSM */
        sapEvent.event = eSAP_HDD_START_INFRA_BSS;
        sapEvent.params = 0;//pSapPhysLinkCreate

        /* Store the HDD callback in SAP context */
        pSapCtx->pfnSapEventCallback = pSapEventCallback;

        /* Handle event*/
        vosStatus = sapFsm(pSapCtx, &sapEvent);
     }
     else
     {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                "SoftAp role has not been enabled");
     }

    return vosStatus;
}// WLANSAP_StartBss

/*==========================================================================
  FUNCTION    WLANSAP_SetMacACL

  DESCRIPTION
    This api function provides SAP to set mac list entry in accept list as well
    as deny list

  DEPENDENCIES

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs
    pQctCommitConfig    : Pointer to configuration structure passed down from
                          HDD(HostApd for Android)

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Pointer to SAP cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_SetMacACL
(
    v_PVOID_t  pCtx,   //pwextCtx
    tsap_Config_t *pConfig
)
{
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    ptSapContext pSapCtx = NULL;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                 "WLANSAP_SetMacACL");

    if (VOS_STA_SAP_MODE == vos_get_conparam ())
    {
        pSapCtx = VOS_GET_SAP_CB(pCtx);
        if ( NULL == pSapCtx )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                       "%s: Invalid SAP pointer from pCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        // Copy MAC filtering settings to sap context
        pSapCtx->eSapMacAddrAclMode = pConfig->SapMacaddr_acl;

        if (eSAP_DENY_UNLESS_ACCEPTED == pSapCtx->eSapMacAddrAclMode)
        {
            vos_mem_copy(pSapCtx->acceptMacList, pConfig->accept_mac,
                                                 sizeof(pConfig->accept_mac));
            pSapCtx->nAcceptMac = pConfig->num_accept_mac;
            sapSortMacList(pSapCtx->acceptMacList, pSapCtx->nAcceptMac);
        }
        else if (eSAP_ACCEPT_UNLESS_DENIED == pSapCtx->eSapMacAddrAclMode)
        {
            vos_mem_copy(pSapCtx->denyMacList, pConfig->deny_mac,
                                               sizeof(pConfig->deny_mac));
            pSapCtx->nDenyMac = pConfig->num_deny_mac;
            sapSortMacList(pSapCtx->denyMacList, pSapCtx->nDenyMac);
        }
    }
    else
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s : SoftAp role has not been enabled", __func__);
        return VOS_STATUS_E_FAULT;
    }

    return vosStatus;
}//WLANSAP_SetMacACL

/*==========================================================================
  FUNCTION    WLANSAP_StopBss

  DESCRIPTION
    This api function provides SAP FSM event eSAP_HDD_STOP_INFRA_BSS for
    stopping AP BSS

  DEPENDENCIES

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Pointer to VOSS GC is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_StopBss
(
    v_PVOID_t  pCtx
)
{
    tWLAN_SAPEvent sapEvent;    /* State machine event*/
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    ptSapContext pSapCtx = NULL;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    /*------------------------------------------------------------------------
        Sanity check
        Extract SAP control block
    ------------------------------------------------------------------------*/
    if ( NULL == pCtx )
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid Global VOSS handle", __func__);
        return VOS_STATUS_E_FAULT;
    }

    pSapCtx = VOS_GET_SAP_CB(pCtx);

    if (NULL == pSapCtx )
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /* Fill in the event structure for FSM */
    sapEvent.event = eSAP_HDD_STOP_INFRA_BSS;
    sapEvent.params = 0;

    /* Handle event*/
    vosStatus = sapFsm(pSapCtx, &sapEvent);

    return vosStatus;
}

/*==========================================================================
  FUNCTION    WLANSAP_GetAssocStations

  DESCRIPTION
    This api function is used to probe the list of associated stations from various modules of CORE stack

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs
    modId       : Module from whom list of associtated stations  is supposed to be probed. If an invalid module is passed
                        then by default VOS_MODULE_ID_PE will be probed
    IN/OUT
    pAssocStas      : Pointer to list of associated stations that are known to the module specified in mod parameter

  NOTE: The memory for this list will be allocated by the caller of this API

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_GetAssocStations
(
    v_PVOID_t  pCtx,
    VOS_MODULE_ID modId,
    tpSap_AssocMacAddr pAssocStas
)
{
    ptSapContext pSapCtx = VOS_GET_SAP_CB(pCtx);

    /*------------------------------------------------------------------------
      Sanity check
      Extract SAP control block
      ------------------------------------------------------------------------*/
    if (NULL == pSapCtx)
    {
      VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                 "%s: Invalid SAP pointer from pCtx", __func__);
      return VOS_STATUS_E_FAULT;
    }

    sme_RoamGetAssociatedStas( VOS_GET_HAL_CB(pSapCtx->pvosGCtx), pSapCtx->sessionId,
                                modId,
                                pSapCtx->pUsrContext,
                                (v_PVOID_t *)pSapCtx->pfnSapEventCallback,
                                (v_U8_t *)pAssocStas );

    return VOS_STATUS_SUCCESS;
}


/*==========================================================================
  FUNCTION    WLANSAP_RemoveWpsSessionOverlap

  DESCRIPTION
    This api function provides for Ap App/HDD to remove an entry from session session overlap info.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs
    pRemoveMac: pointer to v_MACADDR_t for session MAC address

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success
    VOS_STATUS_E_FAULT:  Session is not dectected. The parameter is function not valid.

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_RemoveWpsSessionOverlap

(
    v_PVOID_t  pCtx,
    v_MACADDR_t pRemoveMac
)
{
    ptSapContext pSapCtx = VOS_GET_SAP_CB(pCtx);

  /*------------------------------------------------------------------------
    Sanity check
    Extract SAP control block
  ------------------------------------------------------------------------*/
  if (NULL == pSapCtx)
  {
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
               "%s: Invalid SAP pointer from pCtx", __func__);
    return VOS_STATUS_E_FAULT;
  }

  sme_RoamGetWpsSessionOverlap( VOS_GET_HAL_CB(pSapCtx->pvosGCtx), pSapCtx->sessionId,
                                pSapCtx->pUsrContext,
                                (v_PVOID_t *)pSapCtx->pfnSapEventCallback,
                                pRemoveMac);

  return VOS_STATUS_SUCCESS;
}

/*==========================================================================
  FUNCTION    WLANSAP_getWpsSessionOverlap

  DESCRIPTION
    This api function provides for Ap App/HDD to get WPS session overlap info.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_getWpsSessionOverlap
(
    v_PVOID_t  pCtx
)
{
    v_MACADDR_t pRemoveMac = VOS_MAC_ADDR_ZERO_INITIALIZER;

    ptSapContext pSapCtx = VOS_GET_SAP_CB(pCtx);

    /*------------------------------------------------------------------------
      Sanity check
      Extract SAP control block
      ------------------------------------------------------------------------*/
    if (NULL == pSapCtx)
    {
      VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                 "%s: Invalid SAP pointer from pCtx", __func__);
      return VOS_STATUS_E_FAULT;
    }

    sme_RoamGetWpsSessionOverlap( VOS_GET_HAL_CB(pSapCtx->pvosGCtx), pSapCtx->sessionId,
                                pSapCtx->pUsrContext,
                                (v_PVOID_t *)pSapCtx->pfnSapEventCallback,
                                pRemoveMac);

    return VOS_STATUS_SUCCESS;
}


/* This routine will set the mode of operation for ACL dynamically*/
VOS_STATUS
WLANSAP_SetMode
(
    v_PVOID_t pCtx,
    v_U32_t mode
)
{
    ptSapContext pSapCtx = VOS_GET_SAP_CB(pCtx);

    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    pSapCtx->eSapMacAddrAclMode = (eSapMacAddrACL)mode;
    return VOS_STATUS_SUCCESS;
}

/* Get ACL Mode */
VOS_STATUS
WLANSAP_GetACLMode
(
    v_PVOID_t pCtx,
    eSapMacAddrACL *mode
)
{
    ptSapContext pSapCtx = VOS_GET_SAP_CB(pCtx);

    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    *mode = pSapCtx->eSapMacAddrAclMode;
    return VOS_STATUS_SUCCESS;
}

/* API to get ACL Accept List */
VOS_STATUS
WLANSAP_GetACLAcceptList
(
   v_PVOID_t pCtx,
   v_MACADDR_t *pAcceptList,
   v_U8_t *nAcceptList
)
{
    ptSapContext pSapCtx = VOS_GET_SAP_CB(pCtx);
    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    memcpy((void *)pAcceptList, (void *)pSapCtx->acceptMacList,
                       (pSapCtx->nAcceptMac*sizeof(v_MACADDR_t)));
    *nAcceptList = pSapCtx->nAcceptMac;
    return VOS_STATUS_SUCCESS;
}

/* API to get Deny List */
VOS_STATUS
WLANSAP_GetACLDenyList
(
   v_PVOID_t pCtx,
   v_MACADDR_t *pDenyList,
   v_U8_t *nDenyList
)
{
    ptSapContext pSapCtx = VOS_GET_SAP_CB(pCtx);
    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    memcpy((void *)pDenyList, (void *)pSapCtx->denyMacList,
                         (pSapCtx->nDenyMac*sizeof(v_MACADDR_t)));
    *nDenyList = pSapCtx->nDenyMac;
    return VOS_STATUS_SUCCESS;
}

/* This routine will clear all the entries in accept list as well as deny list  */

VOS_STATUS
WLANSAP_ClearACL
(
    v_PVOID_t pCtx
)
{
    ptSapContext  pSapCtx = VOS_GET_SAP_CB(pCtx);
    v_U8_t i;

    if (NULL == pSapCtx)
    {
        return VOS_STATUS_E_RESOURCES;
    }

    if (pSapCtx->denyMacList != NULL)
    {
        for (i = 0; i < (pSapCtx->nDenyMac-1); i++)
        {
            vos_mem_zero((pSapCtx->denyMacList+i)->bytes, sizeof(v_MACADDR_t));

        }
    }
    sapPrintACL(pSapCtx->denyMacList, pSapCtx->nDenyMac);
    pSapCtx->nDenyMac  = 0;

    if (pSapCtx->acceptMacList!=NULL)
    {
        for (i = 0; i < (pSapCtx->nAcceptMac-1); i++)
        {
            vos_mem_zero((pSapCtx->acceptMacList+i)->bytes, sizeof(v_MACADDR_t));

        }
    }
    sapPrintACL(pSapCtx->acceptMacList, pSapCtx->nAcceptMac);
    pSapCtx->nAcceptMac = 0;

    return VOS_STATUS_SUCCESS;
}

VOS_STATUS
WLANSAP_ModifyACL
(
    v_PVOID_t pCtx,
    v_U8_t *pPeerStaMac,
    eSapACLType listType,
    eSapACLCmdType cmd
)
{
    eSapBool staInWhiteList=eSAP_FALSE, staInBlackList=eSAP_FALSE;
    v_U8_t staWLIndex, staBLIndex;
    ptSapContext  pSapCtx = VOS_GET_SAP_CB(pCtx);

    if (NULL == pSapCtx)
    {
       VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                  "%s: Invalid SAP Context", __func__);
       return VOS_STATUS_E_FAULT;
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_LOW,"Modify ACL entered\n"
            "Before modification of ACL\n"
            "size of accept and deny lists %d %d",
            pSapCtx->nAcceptMac, pSapCtx->nDenyMac);
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,"*** WHITE LIST ***");
    sapPrintACL(pSapCtx->acceptMacList, pSapCtx->nAcceptMac);
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,"*** BLACK LIST ***");
    sapPrintACL(pSapCtx->denyMacList, pSapCtx->nDenyMac);

    /* the expectation is a mac addr will not be in both the lists at the same time.
       It is the responsiblity of userspace to ensure this */
    staInWhiteList = sapSearchMacList(pSapCtx->acceptMacList, pSapCtx->nAcceptMac, pPeerStaMac, &staWLIndex);
    staInBlackList = sapSearchMacList(pSapCtx->denyMacList, pSapCtx->nDenyMac, pPeerStaMac, &staBLIndex);

    if (staInWhiteList && staInBlackList)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                "Peer mac "MAC_ADDRESS_STR" found in white and black lists."
                "Initial lists passed incorrect. Cannot execute this command.",
                MAC_ADDR_ARRAY(pPeerStaMac));
        return VOS_STATUS_E_FAILURE;

    }

    switch(listType)
    {
        case eSAP_WHITE_LIST:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_LOW, "cmd %d", cmd);
            if (cmd == ADD_STA_TO_ACL)
            {
                //error check
                // if list is already at max, return failure
                if (pSapCtx->nAcceptMac == MAX_ACL_MAC_ADDRESS)
                {
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                            "White list is already maxed out. Cannot accept "MAC_ADDRESS_STR,
                            MAC_ADDR_ARRAY(pPeerStaMac));
                    return VOS_STATUS_E_FAILURE;
                }
                if (staInWhiteList)
                {
                    //Do nothing if already present in white list. Just print a warning
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_WARN,
                            "MAC address already present in white list "MAC_ADDRESS_STR,
                            MAC_ADDR_ARRAY(pPeerStaMac));
                } else
                {
                    if (staInBlackList)
                    {
                        //remove it from black list before adding to the white list
                        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_WARN,
                                "STA present in black list so first remove from it");
                        sapRemoveMacFromACL(pSapCtx->denyMacList, &pSapCtx->nDenyMac, staBLIndex);
                    }
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                            "... Now add to the white list");
                    sapAddMacToACL(pSapCtx->acceptMacList, &pSapCtx->nAcceptMac, pPeerStaMac);
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_LOW, "size of accept and deny lists %d %d",
                            pSapCtx->nAcceptMac, pSapCtx->nDenyMac);
                }
            }
            else if (cmd == DELETE_STA_FROM_ACL)
            {
                if (staInWhiteList)
                {
                    struct tagCsrDelStaParams delStaParams;

                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO, "Delete from white list");
                    sapRemoveMacFromACL(pSapCtx->acceptMacList, &pSapCtx->nAcceptMac, staWLIndex);
                    /* If a client is deleted from white list and the client is connected, send deauth*/
                    WLANSAP_PopulateDelStaParams(pPeerStaMac,
                                                  eCsrForcedDeauthSta,
                                                  (SIR_MAC_MGMT_DEAUTH >> 4),
                                                   &delStaParams);
                    WLANSAP_DeauthSta(pCtx, &delStaParams);
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_LOW, "size of accept and deny lists %d %d",
                            pSapCtx->nAcceptMac, pSapCtx->nDenyMac);
                }
                else
                {
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_WARN,
                            "MAC address to be deleted is not present in the white list "MAC_ADDRESS_STR,
                            MAC_ADDR_ARRAY(pPeerStaMac));
                    return VOS_STATUS_E_FAILURE;
                }
            }
            else
            {
                VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR, "Invalid cmd type passed");
                return VOS_STATUS_E_FAILURE;
            }
            break;

        case eSAP_BLACK_LIST:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_LOW,
                    "cmd %d", cmd);
            if (cmd == ADD_STA_TO_ACL)
            {
                //error check
                // if list is already at max, return failure
                if (pSapCtx->nDenyMac == MAX_ACL_MAC_ADDRESS)
                {
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                            "Black list is already maxed out. Cannot accept "MAC_ADDRESS_STR,
                            MAC_ADDR_ARRAY(pPeerStaMac));
                    return VOS_STATUS_E_FAILURE;
                }
                if (staInBlackList)
                {
                    //Do nothing if already present in white list
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_WARN,
                            "MAC address already present in black list "MAC_ADDRESS_STR,
                            MAC_ADDR_ARRAY(pPeerStaMac));
                } else
                {
                    struct tagCsrDelStaParams delStaParams;
                    if (staInWhiteList)
                    {
                        //remove it from white list before adding to the black list
                        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_WARN,
                                "Present in white list so first remove from it");
                        sapRemoveMacFromACL(pSapCtx->acceptMacList, &pSapCtx->nAcceptMac, staWLIndex);
                    }
                    /* If we are adding a client to the black list; if its connected, send deauth */
                    WLANSAP_PopulateDelStaParams(pPeerStaMac,
                                                 eCsrForcedDeauthSta,
                                                 (SIR_MAC_MGMT_DEAUTH >> 4),
                                                 &delStaParams);
                    WLANSAP_DeauthSta(pCtx, &delStaParams);
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                            "... Now add to black list");
                    sapAddMacToACL(pSapCtx->denyMacList, &pSapCtx->nDenyMac, pPeerStaMac);
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_LOW,"size of accept and deny lists %d %d",
                            pSapCtx->nAcceptMac, pSapCtx->nDenyMac);
                }
            }
            else if (cmd == DELETE_STA_FROM_ACL)
            {
                if (staInBlackList)
                {
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO, "Delete from black list");
                    sapRemoveMacFromACL(pSapCtx->denyMacList, &pSapCtx->nDenyMac, staBLIndex);
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_LOW,"no accept and deny mac %d %d",
                            pSapCtx->nAcceptMac, pSapCtx->nDenyMac);
                }
                else
                {
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_WARN,
                            "MAC address to be deleted is not present in the black list "MAC_ADDRESS_STR,
                            MAC_ADDR_ARRAY(pPeerStaMac));
                    return VOS_STATUS_E_FAILURE;
                }
            }
            else
            {
                VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR, "Invalid cmd type passed");
                return VOS_STATUS_E_FAILURE;
            }
            break;

        default:
            {
                VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                        "Invalid list type passed %d",listType);
                return VOS_STATUS_E_FAILURE;
            }
    }
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_LOW,"After modification of ACL");
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,"*** WHITE LIST ***");
    sapPrintACL(pSapCtx->acceptMacList, pSapCtx->nAcceptMac);
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,"*** BLACK LIST ***");
    sapPrintACL(pSapCtx->denyMacList, pSapCtx->nDenyMac);
    return VOS_STATUS_SUCCESS;
}

/*==========================================================================
  FUNCTION    WLANSAP_DisassocSta

  DESCRIPTION
    This api function provides for Ap App/HDD initiated disassociation of station

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs
    pPeerStaMac : Mac address of the station to disassociate

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_DisassocSta
(
    v_PVOID_t pCtx,
    struct tagCsrDelStaParams *pDelStaParams
)
{
    ptSapContext pSapCtx = VOS_GET_SAP_CB(pCtx);

    /*------------------------------------------------------------------------
      Sanity check
      Extract SAP control block
      ------------------------------------------------------------------------*/
    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    sme_RoamDisconnectSta(VOS_GET_HAL_CB(pSapCtx->pvosGCtx), pSapCtx->sessionId,
                            pDelStaParams);

    return VOS_STATUS_SUCCESS;
}

/*==========================================================================
  FUNCTION    WLANSAP_DeauthSta

  DESCRIPTION
    This api function provides for Ap App/HDD initiated deauthentication of station

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs
    pDelStaParams       : Pointer to parameters of the station to
                          deauthenticate

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_DeauthSta
(
    v_PVOID_t pCtx,
    struct tagCsrDelStaParams *pDelStaParams
)
{
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    VOS_STATUS vosStatus = VOS_STATUS_E_FAULT;
    ptSapContext pSapCtx = VOS_GET_SAP_CB(pCtx);

    /*------------------------------------------------------------------------
      Sanity check
      Extract SAP control block
      ------------------------------------------------------------------------*/
    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pCtx", __func__);
        return vosStatus;
    }

    halStatus = sme_RoamDeauthSta(VOS_GET_HAL_CB(pSapCtx->pvosGCtx),
                                  pSapCtx->sessionId, pDelStaParams);

    if (halStatus == eHAL_STATUS_SUCCESS)
    {
        vosStatus = VOS_STATUS_SUCCESS;
    }
    return vosStatus;
}

/*==========================================================================
  FUNCTION    WLANSAP_SetChannelChangeWithCsa

  DESCRIPTION
      This api function does a channel change to the target channel specified
      through an iwpriv. CSA IE is included in the beacons before doing a
      channel change.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pvosGCtx             : Pointer to vos global context structure
    targetChannel        : New target channel to change to.

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_SetChannelChangeWithCsa(v_PVOID_t pvosGCtx, v_U32_t targetChannel)
{

    ptSapContext sapContext = NULL;
    tWLAN_SAPEvent sapEvent;
    tpAniSirGlobal pMac = NULL;
    v_PVOID_t hHal = NULL;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
    bool valid;
#endif
    tSmeConfigParams  sme_config;

    sapContext = VOS_GET_SAP_CB( pvosGCtx );
    if (NULL == sapContext)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);

        return VOS_STATUS_E_FAULT;
    }
    hHal = VOS_GET_HAL_CB(sapContext->pvosGCtx);
    if (NULL == hHal)
    {
       VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
       "%s: Invalid HAL pointer from pvosGCtx", __func__);
       return VOS_STATUS_E_FAULT;
    }
    pMac = PMAC_STRUCT( hHal );

    /*
     * Now, validate if the passed channel is valid in the
     * current regulatory domain.
     */
    if ( sapContext->channel != targetChannel &&
         ((vos_nv_getChannelEnabledState(targetChannel) == NV_CHANNEL_ENABLE)
                                    ||
         (vos_nv_getChannelEnabledState(targetChannel) == NV_CHANNEL_DFS &&
          !vos_concurrent_open_sessions_running())) )
    {
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
         /*
          * validate target channel switch w.r.t various concurrency rules set.
          */
         valid = sme_validate_sap_channel_switch(VOS_GET_HAL_CB(sapContext->pvosGCtx),
                  targetChannel, sapContext->csrRoamProfile.phyMode,
                  sapContext->cc_switch_mode, sapContext->sessionId);
         if (!valid)
         {
             VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       FL("Channel switch to %u is not allowed due to concurrent channel interference"),
                          targetChannel);
             return VOS_STATUS_E_FAULT;
         }
#endif
        /*
         * Post a CSA IE request to SAP state machine with
         * target channel information and also CSA IE required
         * flag set in sapContext only, if SAP is in eSAP_STARTED
         * state.
         */
         if (eSAP_STARTED == sapContext->sapsMachine)
         {
             /*
              * currently OBSS scan is done in hostapd, so to avoid
              * SAP coming up in HT40 on channel switch we are
              * disabling channel bonding in 2.4ghz.
              */
             if (targetChannel <= RF_CHAN_14)
             {
                 sme_GetConfigParam(pMac, &sme_config);
                 sme_config.csrConfig.channelBondingMode24GHz =
                                         eCSR_INI_SINGLE_CHANNEL_CENTERED;
                 sme_UpdateConfig(pMac, &sme_config);
             }

             /*
              * Copy the requested target channel
              * to sap context.
              */
             pMac->sap.SapDfsInfo.target_channel = targetChannel;
             pMac->sap.SapDfsInfo.new_chanWidth =
                                   sapContext->ch_width_orig;

             /*
              * Set the CSA IE required flag.
              */
             pMac->sap.SapDfsInfo.csaIERequired = VOS_TRUE;

             /*
              * Set the radar found status to allow the channel
              * change to happen same as in the case of a radar
              * detection. Since, this will allow SAP to be in
              * correct state and also resume the netif queues
              * that were suspended in HDD before the channel
              * request was issued.
              */
             pMac->sap.SapDfsInfo.sap_radar_found_status = VOS_TRUE;
             pMac->sap.SapDfsInfo.cac_state = eSAP_DFS_DO_NOT_SKIP_CAC;
             sap_CacResetNotify(hHal);

             /*
              * Post the eSAP_DFS_CHNL_SWITCH_ANNOUNCEMENT_START
              * to SAP state machine to process the channel
              * request with CSA IE set in the beacons.
              */
             sapEvent.event = eSAP_DFS_CHNL_SWITCH_ANNOUNCEMENT_START;
             sapEvent.params = 0;
             sapEvent.u1 = 0;
             sapEvent.u2 = 0;

             sapFsm(sapContext, &sapEvent);

         }
         else
         {
              VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Failed to request Channel Change, since"
                   "SAP is not in eSAP_STARTED state", __func__);
              return VOS_STATUS_E_FAULT;
         }

    }
    else
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Channel = %d is not valid in the current"
                   "regulatory domain",
                    __func__, targetChannel);

        return VOS_STATUS_E_FAULT;
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "%s: Posted eSAP_DFS_CHNL_SWITCH_ANNOUNCEMENT_START"
                   "successfully to sapFsm for Channel = %d",
                    __func__, targetChannel);

    return VOS_STATUS_SUCCESS;
}

/*==========================================================================
  FUNCTION    WLANSAP_SetCounterMeasure

  DESCRIPTION
    This api function is used to disassociate all the stations and prevent
    association for any other station.Whenever Authenticator receives 2 mic failures
    within 60 seconds, Authenticator will enable counter measure at SAP Layer.
    Authenticator will start the 60 seconds timer. Core stack will not allow any
    STA to associate till HDD disables counter meassure. Core stack shall kick out all the
    STA which are currently associated and DIASSOC Event will be propogated to HDD for
    each STA to clean up the HDD STA table.Once the 60 seconds timer expires, Authenticator
    will disable the counter meassure at core stack. Now core stack can allow STAs to associate.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs
    bEnable: If TRUE than all stations will be disassociated and no more
                        will be allowed to associate. If FALSE than CORE
                        will come out of this state.

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_SetCounterMeasure
(
    v_PVOID_t pCtx,
    v_BOOL_t bEnable
)
{
    ptSapContext  pSapCtx = VOS_GET_SAP_CB(pCtx);

    /*------------------------------------------------------------------------
      Sanity check
      Extract SAP control block
      ------------------------------------------------------------------------*/
    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    sme_RoamTKIPCounterMeasures(VOS_GET_HAL_CB(pSapCtx->pvosGCtx), pSapCtx->sessionId, bEnable);

    return VOS_STATUS_SUCCESS;
}

/*==========================================================================

  FUNCTION    WLANSAP_SetKeysSta

  DESCRIPTION
    This api function provides for Ap App/HDD to set key for a station.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs
    pSetKeyInfo : tCsrRoamSetKey structure for the station

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_SetKeySta
(
    v_PVOID_t pCtx,
    tCsrRoamSetKey *pSetKeyInfo
)
{
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    ptSapContext pSapCtx = NULL;
    v_PVOID_t hHal = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    v_U32_t roamId=0xFF;

    if (VOS_STA_SAP_MODE == vos_get_conparam ( ))
    {
        pSapCtx = VOS_GET_SAP_CB(pCtx);
        if (NULL == pSapCtx)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if (NULL == hHal)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid HAL pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        halStatus = sme_RoamSetKey(hHal, pSapCtx->sessionId, pSetKeyInfo, &roamId);

        if (halStatus == eHAL_STATUS_SUCCESS)
        {
            vosStatus = VOS_STATUS_SUCCESS;
        } else
        {
            vosStatus = VOS_STATUS_E_FAULT;
        }
    }
    else
        vosStatus = VOS_STATUS_E_FAULT;

    return vosStatus;
}

/*==========================================================================
  FUNCTION    WLANSAP_DelKeySta

  DESCRIPTION
    This api function provides for Ap App/HDD to delete key for a station.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs
    pSetKeyInfo : tCsrRoamRemoveKey structure for the station

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_DelKeySta
(
    v_PVOID_t pCtx,
    tCsrRoamRemoveKey *pRemoveKeyInfo
)
{
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    ptSapContext pSapCtx = NULL;
    v_PVOID_t hHal = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    v_U32_t roamId=0xFF;
    tCsrRoamRemoveKey RemoveKeyInfo;

    if (VOS_STA_SAP_MODE == vos_get_conparam ( ))
    {
        pSapCtx = VOS_GET_SAP_CB(pCtx);
        if (NULL == pSapCtx)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if (NULL == hHal)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid HAL pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        vos_mem_zero(&RemoveKeyInfo, sizeof(RemoveKeyInfo));
        RemoveKeyInfo.encType = pRemoveKeyInfo->encType;
        vos_mem_copy(RemoveKeyInfo.peerMac, pRemoveKeyInfo->peerMac, VOS_MAC_ADDR_SIZE);
        RemoveKeyInfo.keyId = pRemoveKeyInfo->keyId;

        halStatus = sme_RoamRemoveKey(hHal, pSapCtx->sessionId, &RemoveKeyInfo, &roamId);

        if (HAL_STATUS_SUCCESS(halStatus))
        {
            vosStatus = VOS_STATUS_SUCCESS;
        }
        else
        {
            vosStatus = VOS_STATUS_E_FAULT;
        }
    }
    else
        vosStatus = VOS_STATUS_E_FAULT;

    return vosStatus;
}

VOS_STATUS
WLANSap_getstationIE_information
(
    v_PVOID_t pCtx,
    v_U32_t   *pLen,
    v_U8_t    *pBuf
)
{
    VOS_STATUS vosStatus = VOS_STATUS_E_FAILURE;
    ptSapContext pSapCtx = NULL;
    v_U32_t len = 0;

    if (VOS_STA_SAP_MODE == vos_get_conparam ( )){
        pSapCtx = VOS_GET_SAP_CB(pCtx);
        if (NULL == pSapCtx)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        if (pLen)
        {
            len = *pLen;
            *pLen = pSapCtx->nStaWPARSnReqIeLength;
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                       "%s: WPAIE len : %x", __func__, *pLen);
            if(pBuf)
            {
                if(len >= pSapCtx->nStaWPARSnReqIeLength)
                {
                    vos_mem_copy( pBuf, pSapCtx->pStaWpaRsnReqIE, pSapCtx->nStaWPARSnReqIeLength);
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                               "%s: WPAIE: %02x:%02x:%02x:%02x:%02x:%02x",
                               __func__,
                               pBuf[0], pBuf[1], pBuf[2],
                               pBuf[3], pBuf[4], pBuf[5]);
                    vosStatus = VOS_STATUS_SUCCESS;
                }
            }
        }
    }

    if( VOS_STATUS_E_FAILURE == vosStatus)
    {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                  "%s: Error unable to populate the RSNWPAIE",
                  __func__);
    }

    return vosStatus;

}

/*==========================================================================
  FUNCTION    WLANSAP_Set_WpsIe

  DESCRIPTION
    This api function provides for Ap App/HDD to set WPS IE.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs
    pWPSIE      : tSap_WPSIE structure that include WPS IEs

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_Set_WpsIe
(
    v_PVOID_t pCtx,
    tSap_WPSIE *pSap_WPSIe
)
{
    ptSapContext pSapCtx = NULL;
    v_PVOID_t hHal = NULL;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
            "%s, %d", __func__, __LINE__);

    if(VOS_STA_SAP_MODE == vos_get_conparam ( )) {
        pSapCtx = VOS_GET_SAP_CB(pCtx);
        if ( NULL == pSapCtx )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if ( NULL == hHal ){
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid HAL pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        if ( sap_AcquireGlobalLock( pSapCtx ) == VOS_STATUS_SUCCESS )
        {
            if (pSap_WPSIe->sapWPSIECode == eSAP_WPS_BEACON_IE)
            {
                vos_mem_copy(&pSapCtx->APWPSIEs.SirWPSBeaconIE, &pSap_WPSIe->sapwpsie.sapWPSBeaconIE, sizeof(tSap_WPSBeaconIE));
            }
            else if (pSap_WPSIe->sapWPSIECode == eSAP_WPS_PROBE_RSP_IE)
            {
                vos_mem_copy(&pSapCtx->APWPSIEs.SirWPSProbeRspIE, &pSap_WPSIe->sapwpsie.sapWPSProbeRspIE, sizeof(tSap_WPSProbeRspIE));
            }
            else
            {
                sap_ReleaseGlobalLock( pSapCtx );
                return VOS_STATUS_E_FAULT;
            }
            sap_ReleaseGlobalLock( pSapCtx );
            return VOS_STATUS_SUCCESS;
        }
        else
            return VOS_STATUS_E_FAULT;
    }
    else
        return VOS_STATUS_E_FAULT;
}

/*==========================================================================
  FUNCTION   WLANSAP_Update_WpsIe

  DESCRIPTION
    This api function provides for Ap App/HDD to update WPS IEs.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_Update_WpsIe
(
    v_PVOID_t pCtx
)
{
    VOS_STATUS vosStatus = VOS_STATUS_E_FAULT;
    ptSapContext pSapCtx = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    v_PVOID_t hHal = NULL;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
            "%s, %d", __func__, __LINE__);

    if(VOS_STA_SAP_MODE == vos_get_conparam ( )){
        pSapCtx = VOS_GET_SAP_CB(pCtx);
        if ( NULL == pSapCtx )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if ( NULL == hHal ){
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid HAL pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        halStatus = sme_RoamUpdateAPWPSIE( hHal, pSapCtx->sessionId, &pSapCtx->APWPSIEs);

        if(halStatus == eHAL_STATUS_SUCCESS) {
            vosStatus = VOS_STATUS_SUCCESS;
        } else
        {
            vosStatus = VOS_STATUS_E_FAULT;
        }

    }

    return vosStatus;
}

/*==========================================================================
  FUNCTION    WLANSAP_Get_WPS_State

  DESCRIPTION
    This api function provides for Ap App/HDD to check if WPS session in process.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs

    OUT
    pbWPSState: Pointer to variable to indicate if it is in WPS Registration state

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_Get_WPS_State
(
    v_PVOID_t pCtx,
    v_BOOL_t *bWPSState
)
{
    ptSapContext pSapCtx = NULL;
    v_PVOID_t hHal = NULL;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
        "%s, %d", __func__, __LINE__);

    if(VOS_STA_SAP_MODE == vos_get_conparam ( )){

        pSapCtx = VOS_GET_SAP_CB(pCtx);
        if ( NULL == pSapCtx )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pCtx", __func__);
             return VOS_STATUS_E_FAULT;
        }

        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if ( NULL == hHal ){
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid HAL pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        if ( sap_AcquireGlobalLock(pSapCtx ) == VOS_STATUS_SUCCESS )
        {
            if(pSapCtx->APWPSIEs.SirWPSProbeRspIE.FieldPresent & SIR_WPS_PROBRSP_SELECTEDREGISTRA_PRESENT)
                *bWPSState = eANI_BOOLEAN_TRUE;
            else
                *bWPSState = eANI_BOOLEAN_FALSE;

            sap_ReleaseGlobalLock( pSapCtx  );

            return VOS_STATUS_SUCCESS;
        }
        else
            return VOS_STATUS_E_FAULT;
    }
    else
        return VOS_STATUS_E_FAULT;

}

VOS_STATUS
sap_AcquireGlobalLock
(
    ptSapContext  pSapCtx
)
{
    VOS_STATUS vosStatus = VOS_STATUS_E_FAULT;

    if( VOS_IS_STATUS_SUCCESS( vos_lock_acquire( &pSapCtx->SapGlobalLock) ) )
    {
            vosStatus = VOS_STATUS_SUCCESS;
    }

    return (vosStatus);
}

VOS_STATUS
sap_ReleaseGlobalLock
(
    ptSapContext  pSapCtx
)
{
    VOS_STATUS vosStatus = VOS_STATUS_E_FAULT;

    if( VOS_IS_STATUS_SUCCESS( vos_lock_release( &pSapCtx->SapGlobalLock) ) )
    {
        vosStatus = VOS_STATUS_SUCCESS;
    }

    return (vosStatus);
}

/*==========================================================================
  FUNCTION    WLANSAP_Set_WPARSNIes

  DESCRIPTION
    This api function provides for Ap App/HDD to set AP WPA and RSN IE in its beacon and probe response.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs
    pWPARSNIEs  : buffer to the WPA/RSN IEs
    WPARSNIEsLen: length of WPA/RSN IEs

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS WLANSAP_Set_WPARSNIes
(
    v_PVOID_t pCtx,
    v_U8_t *pWPARSNIEs,
    v_U32_t WPARSNIEsLen
)
{
    ptSapContext pSapCtx = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    v_PVOID_t hHal = NULL;

    if(VOS_STA_SAP_MODE == vos_get_conparam ( )){
        pSapCtx = VOS_GET_SAP_CB(pCtx);
        if ( NULL == pSapCtx )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if ( NULL == hHal ){
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid HAL pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        pSapCtx->APWPARSNIEs.length = (tANI_U16)WPARSNIEsLen;
        vos_mem_copy(pSapCtx->APWPARSNIEs.rsnIEdata, pWPARSNIEs, WPARSNIEsLen);

        halStatus = sme_RoamUpdateAPWPARSNIEs( hHal, pSapCtx->sessionId, &pSapCtx->APWPARSNIEs);

        if(halStatus == eHAL_STATUS_SUCCESS) {
            return VOS_STATUS_SUCCESS;
        } else
        {
            return VOS_STATUS_E_FAULT;
        }
    }

    return VOS_STATUS_E_FAULT;
}

VOS_STATUS WLANSAP_GetStatistics
(
    v_PVOID_t pCtx,
    tSap_SoftapStats *statBuf,
    v_BOOL_t bReset
)
{
    if (NULL == pCtx)
    {
        return VOS_STATUS_E_FAULT;
    }

    return (WLANTL_GetSoftAPStatistics(pCtx, statBuf, bReset));
}

/*==========================================================================

  FUNCTION    WLANSAP_SendAction

  DESCRIPTION
    This api function provides to send action frame sent by upper layer.

  DEPENDENCIES
    NA.

  PARAMETERS

  IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs
    pBuf: Pointer of the action frame to be transmitted
    len: Length of the action frame

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS WLANSAP_SendAction
(
    v_PVOID_t pCtx,
    const tANI_U8 *pBuf,
    tANI_U32 len, tANI_U16 wait
)
{
    ptSapContext pSapCtx = NULL;
    v_PVOID_t hHal = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;

    if( VOS_STA_SAP_MODE == vos_get_conparam ( ) )
    {
        pSapCtx = VOS_GET_SAP_CB(pCtx);
        if (NULL == pSapCtx)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if( ( NULL == hHal ) || ( eSAP_TRUE != pSapCtx->isSapSessionOpen ) )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: HAL pointer (%p) NULL OR SME session is not open (%d)",
                       __func__, hHal, pSapCtx->isSapSessionOpen );
            return VOS_STATUS_E_FAULT;
        }

        halStatus = sme_sendAction( hHal, pSapCtx->sessionId, pBuf, len, 0 , 0);

        if ( eHAL_STATUS_SUCCESS == halStatus )
        {
            return VOS_STATUS_SUCCESS;
        }
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
               "Failed to Send Action Frame");

    return VOS_STATUS_E_FAULT;
}

/*==========================================================================

  FUNCTION    WLANSAP_RemainOnChannel

  DESCRIPTION
    This api function provides to set Remain On channel on specified channel
    for specified duration.

  DEPENDENCIES
    NA.

  PARAMETERS

  IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs
    channel: Channel on which driver has to listen
    duration: Duration for which driver has to listen on specified channel
    callback: Callback function to be called once Listen is done.
    pContext: Context needs to be called in callback function.

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS WLANSAP_RemainOnChannel
(
    v_PVOID_t pCtx,
    tANI_U8 channel,
    tANI_U32 duration,
    remainOnChanCallback callback,
    void *pContext
)
{
    ptSapContext pSapCtx = NULL;
    v_PVOID_t hHal = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;

    if( VOS_STA_SAP_MODE == vos_get_conparam ( ) )
    {
        pSapCtx = VOS_GET_SAP_CB(pCtx);
        if (NULL == pSapCtx)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if( ( NULL == hHal ) || ( eSAP_TRUE != pSapCtx->isSapSessionOpen ) )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: HAL pointer (%p) NULL OR SME session is not open (%d)",
                       __func__, hHal, pSapCtx->isSapSessionOpen );
            return VOS_STATUS_E_FAULT;
        }

        halStatus = sme_RemainOnChannel( hHal, pSapCtx->sessionId,
                          channel, duration, callback, pContext, TRUE );

        if( eHAL_STATUS_SUCCESS == halStatus )
        {
            return VOS_STATUS_SUCCESS;
        }
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
               "Failed to Set Remain on Channel");

    return VOS_STATUS_E_FAULT;
}

/*==========================================================================

  FUNCTION    WLANSAP_CancelRemainOnChannel

  DESCRIPTION
    This api cancel previous remain on channel request.

  DEPENDENCIES
    NA.

  PARAMETERS

  IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS WLANSAP_CancelRemainOnChannel
(
    v_PVOID_t pCtx
)
{
    ptSapContext pSapCtx = NULL;
    v_PVOID_t hHal = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;

    if( VOS_STA_SAP_MODE == vos_get_conparam ( ) )
    {
        pSapCtx = VOS_GET_SAP_CB(pCtx);
        if (NULL == pSapCtx)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if( ( NULL == hHal ) || ( eSAP_TRUE != pSapCtx->isSapSessionOpen ) )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: HAL pointer (%p) NULL OR SME session is not open (%d)",
                       __func__, hHal, pSapCtx->isSapSessionOpen );
            return VOS_STATUS_E_FAULT;
        }

        halStatus = sme_CancelRemainOnChannel( hHal, pSapCtx->sessionId );

        if( eHAL_STATUS_SUCCESS == halStatus )
        {
            return VOS_STATUS_SUCCESS;
        }
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                    "Failed to Cancel Remain on Channel");

    return VOS_STATUS_E_FAULT;
}

/*==========================================================================

  FUNCTION    WLANSAP_RegisterMgmtFrame

  DESCRIPTION
    HDD use this API to register specified type of frame with CORE stack.
    On receiving such kind of frame CORE stack should pass this frame to HDD

  DEPENDENCIES
    NA.

  PARAMETERS

  IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs
    frameType: frameType that needs to be registered with PE.
    matchData: Data pointer which should be matched after frame type is matched.
    matchLen: Length of the matchData

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS WLANSAP_RegisterMgmtFrame
(
    v_PVOID_t pCtx,
    tANI_U16 frameType,
    tANI_U8* matchData,
    tANI_U16 matchLen
)
{
    ptSapContext pSapCtx = NULL;
    v_PVOID_t hHal = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;

    if( VOS_STA_SAP_MODE == vos_get_conparam ( ) )
    {
        pSapCtx = VOS_GET_SAP_CB(pCtx);
        if (NULL == pSapCtx)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if( ( NULL == hHal ) || ( eSAP_TRUE != pSapCtx->isSapSessionOpen ) )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: HAL pointer (%p) NULL OR SME session is not open (%d)",
                       __func__, hHal, pSapCtx->isSapSessionOpen );
            return VOS_STATUS_E_FAULT;
        }

        halStatus = sme_RegisterMgmtFrame(hHal, pSapCtx->sessionId,
                          frameType, matchData, matchLen);

        if( eHAL_STATUS_SUCCESS == halStatus )
        {
            return VOS_STATUS_SUCCESS;
        }
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                    "Failed to Register MGMT frame");

    return VOS_STATUS_E_FAULT;
}

/*==========================================================================

  FUNCTION    WLANSAP_DeRegisterMgmtFrame

  DESCRIPTION
   This API is used to deregister previously registered frame.

  DEPENDENCIES
    NA.

  PARAMETERS

  IN
    pCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context
                  When MBSSID feature is enabled, SAP context is directly
                  passed to SAP APIs
    frameType: frameType that needs to be De-registered with PE.
    matchData: Data pointer which should be matched after frame type is matched.
    matchLen: Length of the matchData

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS WLANSAP_DeRegisterMgmtFrame
(
    v_PVOID_t pCtx,
    tANI_U16 frameType,
    tANI_U8* matchData,
    tANI_U16 matchLen
)
{
    ptSapContext pSapCtx = NULL;
    v_PVOID_t hHal = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;

    if( VOS_STA_SAP_MODE == vos_get_conparam ( ) )
    {
        pSapCtx = VOS_GET_SAP_CB(pCtx);
        if (NULL == pSapCtx)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if( ( NULL == hHal ) || ( eSAP_TRUE != pSapCtx->isSapSessionOpen ) )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: HAL pointer (%p) NULL OR SME session is not open (%d)",
                       __func__, hHal, pSapCtx->isSapSessionOpen );
            return VOS_STATUS_E_FAULT;
        }

        halStatus = sme_DeregisterMgmtFrame( hHal, pSapCtx->sessionId,
                          frameType, matchData, matchLen );

        if( eHAL_STATUS_SUCCESS == halStatus )
        {
            return VOS_STATUS_SUCCESS;
        }
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                    "Failed to Deregister MGMT frame");

    return VOS_STATUS_E_FAULT;
}

/*==========================================================================
  FUNCTION   WLANSAP_ChannelChangeRequest

  DESCRIPTION
   This API is used to send an Indication to SME/PE to change the
   current operating channel to a different target channel.

   The Channel change will be issued by SAP under the following
   scenarios.
   1. A radar indication is received  during SAP CAC WAIT STATE and
      channel change is required.
   2. A radar indication is received during SAP STARTED STATE and
      channel change is required.
  DEPENDENCIES
   NA.

  PARAMETERS
  IN
  pSapCtx: Pointer to vos global context structure

  RETURN VALUE
  The VOS_STATUS code associated with performing the operation

  VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_ChannelChangeRequest(v_PVOID_t pSapCtx, uint8_t target_channel)
{
    ptSapContext sapContext = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    v_PVOID_t hHal = NULL;
    tpAniSirGlobal pMac = NULL;
    eCsrPhyMode phyMode;
    tANI_U32 cbMode;
    uint16_t vhtChannelWidth;
    sapContext = (ptSapContext)pSapCtx;

    if ( NULL == sapContext )
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer", __func__);
        return VOS_STATUS_E_FAULT;
    }

    hHal = VOS_GET_HAL_CB(sapContext->pvosGCtx);
    if (NULL == hHal)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid HAL pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }
    pMac = PMAC_STRUCT( hHal );
    phyMode = sapContext->csrRoamProfile.phyMode;
    sapContext->csrRoamProfile.ChannelInfo.ChannelList[0] = target_channel;
    /*
     * We are getting channel bonding mode from sapDfsInfor structure
     * because we've implemented channel width fallback mechanism for DFS
     * which will result in width of channel changing dynamically.
     */
    cbMode = pMac->sap.SapDfsInfo.new_cbMode;
    vhtChannelWidth = pMac->sap.SapDfsInfo.new_chanWidth;
    sme_SelectCBMode(hHal, phyMode, target_channel, 0, &vhtChannelWidth,
                                         pMac->sap.SapDfsInfo.new_chanWidth);
    sapContext->csrRoamProfile.vht_channel_width = vhtChannelWidth;
    sapContext->vht_channel_width = vhtChannelWidth;
    /* Update the channel as this will be used to
     * send event to supplicant
     */
    sapContext->channel = target_channel;
    halStatus = sme_RoamChannelChangeReq(hHal, sapContext->bssid,
                                         cbMode, &sapContext->csrRoamProfile);

    if (halStatus == eHAL_STATUS_SUCCESS)
    {
        sapSignalHDDevent(sapContext, NULL, eSAP_CHANNEL_CHANGE_EVENT,
                (v_PVOID_t) eSAP_STATUS_SUCCESS);

        return VOS_STATUS_SUCCESS;
    }
    return VOS_STATUS_E_FAULT;
}

/*==========================================================================

 FUNCTION    WLANSAP_StartBeaconReq
 DESCRIPTION
  This API is used to send an Indication to SME/PE to start
  beaconing on the current operating channel.

  Brief:When SAP is started on DFS channel and when ADD BSS RESP is received
  LIM temporarily holds off Beaconing for SAP to do CAC WAIT. When
  CAC WAIT is done SAP resumes the Beacon Tx by sending a start beacon
  request to LIM.

 DEPENDENCIES
  NA.

PARAMETERS

IN
  pSapCtx: Pointer to vos global context structure

RETURN VALUE
  The VOS_STATUS code associated with performing the operation

VOS_STATUS_SUCCESS:  Success

SIDE EFFECTS
============================================================================*/
VOS_STATUS WLANSAP_StartBeaconReq(v_PVOID_t pSapCtx)
{
    ptSapContext sapContext = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    v_PVOID_t hHal = NULL;
    tANI_U8 dfsCacWaitStatus = 0;
    tpAniSirGlobal pMac = NULL;
    sapContext = (ptSapContext)pSapCtx;

    if ( NULL == sapContext )
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer", __func__);
        return VOS_STATUS_E_FAULT;
    }

    hHal = VOS_GET_HAL_CB(sapContext->pvosGCtx);
    if (NULL == hHal)
    {
       VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
       "%s: Invalid HAL pointer from pvosGCtx", __func__);
       return VOS_STATUS_E_FAULT;
    }
    pMac = PMAC_STRUCT( hHal );

    /* No Radar was found during CAC WAIT, So start Beaconing */
    if (pMac->sap.SapDfsInfo.sap_radar_found_status == VOS_FALSE)
    {
       /* CAC Wait done without any Radar Detection */
       dfsCacWaitStatus = VOS_TRUE;
       halStatus = sme_RoamStartBeaconReq( hHal,
                   sapContext->bssid, dfsCacWaitStatus);
       if (halStatus == eHAL_STATUS_SUCCESS)
       {
           return VOS_STATUS_SUCCESS;
       }
       return VOS_STATUS_E_FAULT;
    }

    return VOS_STATUS_E_FAULT;
}


/*==========================================================================
  FUNCTION    WLANSAP_DfsSendCSAIeRequest

  DESCRIPTION
   This API is used to send channel switch announcement request to PE
  DEPENDENCIES
   NA.

  PARAMETERS
  IN
  pSapCtx: Pointer to vos global context structure

  RETURN VALUE
  The VOS_STATUS code associated with performing the operation

  VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_DfsSendCSAIeRequest(v_PVOID_t pSapCtx)
{
    ptSapContext sapContext = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    v_PVOID_t hHal = NULL;
    tpAniSirGlobal pMac = NULL;
    uint16_t cbmode, vht_ch_width;
    uint8_t ch_bandwidth;

    sapContext = (ptSapContext)pSapCtx;

    if ( NULL == sapContext )
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer", __func__);
        return VOS_STATUS_E_FAULT;
    }

    hHal = VOS_GET_HAL_CB(sapContext->pvosGCtx);
    if (NULL == hHal)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid HAL pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    pMac = PMAC_STRUCT( hHal );

    vht_ch_width = pMac->sap.SapDfsInfo.new_chanWidth;
    cbmode = sme_SelectCBMode(hHal,
                     sapContext->csrRoamProfile.phyMode,
                     pMac->sap.SapDfsInfo.target_channel, 0,
                     &vht_ch_width, sapContext->ch_width_orig);

    if (pMac->sap.SapDfsInfo.target_channel <= 14 ||
        vht_ch_width == eHT_CHANNEL_WIDTH_40MHZ ||
        vht_ch_width == eHT_CHANNEL_WIDTH_20MHZ)
    {
        switch (cbmode)
        {
          case eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY:
              ch_bandwidth = BW40_HIGH_PRIMARY;
              break;
          case eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY:
              ch_bandwidth = BW40_LOW_PRIMARY;
              break;
          case eCSR_INI_SINGLE_CHANNEL_CENTERED:
          default:
              ch_bandwidth = BW20;
              break;
        }
    }
    else
        ch_bandwidth = BW80;

    halStatus = sme_RoamCsaIeRequest(hHal,
                                     sapContext->bssid,
                                     pMac->sap.SapDfsInfo.target_channel,
                                     pMac->sap.SapDfsInfo.csaIERequired,
                                     ch_bandwidth);

    if (halStatus == eHAL_STATUS_SUCCESS)
    {
        return VOS_STATUS_SUCCESS;
    }

    return VOS_STATUS_E_FAULT;
}

/*==========================================================================
  FUNCTION    WLANSAP_Get_Dfs_Ignore_CAC

  DESCRIPTION
   This API is used to get the value of ignore_cac value

  DEPENDENCIES
   NA.

  PARAMETERS
  IN
  hHal : HAL pointer
  pIgnore_cac : pointer to ignore_cac variable

  RETURN VALUE
  The VOS_STATUS code associated with performing the operation

  VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS WLANSAP_Get_Dfs_Ignore_CAC(tHalHandle hHal,  v_U8_t *pIgnore_cac)
{
    tpAniSirGlobal pMac = NULL;

    if (NULL != hHal)
    {
        pMac = PMAC_STRUCT( hHal );
    }
    else
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                  "%s: Invalid hHal pointer", __func__);
        return VOS_STATUS_E_FAULT;
    }

    *pIgnore_cac = pMac->sap.SapDfsInfo.ignore_cac;
    return VOS_STATUS_SUCCESS;
}

/*==========================================================================
  FUNCTION    WLANSAP_Set_Dfs_Ignore_CAC

  DESCRIPTION
   This API is used to Set the value of ignore_cac value

  DEPENDENCIES
   NA.

  PARAMETERS
  IN
  hHal : HAL pointer
  ignore_cac : value to set for ignore_cac variable in DFS global structure.

  RETURN VALUE
  The VOS_STATUS code associated with performing the operation

  VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS WLANSAP_Set_Dfs_Ignore_CAC(tHalHandle hHal, v_U8_t ignore_cac)
{
    tpAniSirGlobal pMac = NULL;

    if (NULL != hHal)
    {
        pMac = PMAC_STRUCT( hHal );
    }
    else
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                  "%s: Invalid hHal pointer", __func__);
        return VOS_STATUS_E_FAULT;
    }

    pMac->sap.SapDfsInfo.ignore_cac = (ignore_cac >= VOS_TRUE)?
                                       VOS_TRUE : VOS_FALSE;
    return VOS_STATUS_SUCCESS;
}

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
/**
 * wlan_sap_set_channel_avoidance() - sets sap mcc channel avoidance ini param
 * @hal:                        hal handle
 * @sap_channel_avoidance:      ini parameter value
 *
 * sets sap mcc channel avoidance ini param, to be called in sap_start
 *
 * Return: success of failure of operation
 */
VOS_STATUS
wlan_sap_set_channel_avoidance(tHalHandle hal, bool sap_channel_avoidance)
{
	tpAniSirGlobal mac_ctx = NULL;

	if (NULL != hal)
		mac_ctx = PMAC_STRUCT(hal);

	if (mac_ctx == NULL || hal == NULL) {
		VOS_TRACE(VOS_MODULE_ID_SAP,
			  VOS_TRACE_LEVEL_ERROR,
			  FL("hal or mac_ctx pointer NULL"));
		return VOS_STATUS_E_FAULT;
	}
	mac_ctx->sap.sap_channel_avoidance = sap_channel_avoidance;
	return VOS_STATUS_SUCCESS;
}
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

/*==========================================================================
  FUNCTION    WLANSAP_set_Dfs_Restrict_JapanW53

  DESCRIPTION
   This API is used to enable or disable Japan W53 Band

  DEPENDENCIES
   NA.

  PARAMETERS
  IN
  hHal : HAL pointer
  disable_Dfs_JapanW3 :Indicates if Japan W53 is disabled when set to 1
                       Indicates if Japan W53 is enabled when set to 0

  RETURN VALUE
  The VOS_STATUS code associated with performing the operation

  VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_set_Dfs_Restrict_JapanW53(tHalHandle hHal, v_U8_t disable_Dfs_W53)
{
    tpAniSirGlobal pMac = NULL;
    VOS_STATUS status;
    uint8_t dfs_region;

    if (NULL != hHal)
    {
        pMac = PMAC_STRUCT( hHal );
    }
    else
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                  "%s: Invalid hHal pointer", __func__);
        return VOS_STATUS_E_FAULT;
    }

    vos_nv_get_dfs_region(&dfs_region);

    /*
     * Set the JAPAN W53 restriction only if the current
     * regulatory domain is JAPAN.
     */
    if (DFS_MKK4_DOMAIN == dfs_region)
    {
        pMac->sap.SapDfsInfo.is_dfs_w53_disabled = disable_Dfs_W53;
        VOS_TRACE(VOS_MODULE_ID_SAP,
                  VOS_TRACE_LEVEL_INFO_LOW,
                  FL("sapdfs: SET DFS JAPAN W53 DISABLED = %d"),
                  pMac->sap.SapDfsInfo.is_dfs_w53_disabled);

        status = VOS_STATUS_SUCCESS;
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                  FL("Regdomain not japan, set disable JP W53 not valid"));

        status = VOS_STATUS_E_FAULT;
    }

    return status;
}

/*==========================================================================
  FUNCTION    WLANSAP_set_Dfs_Preferred_Channel_location

  DESCRIPTION
   This API is used to set sap preferred channels location
   to resetrict the DFS random channel selection algorithm
   either Indoor/Outdoor channels only.

  DEPENDENCIES
   NA.

  PARAMETERS
  IN
  hHal : HAL pointer
  dfs_Preferred_Channels_location :
                       0 - Indicates No preferred channel location restrictions
                       1 - Indicates SAP Indoor Channels operation only.
                       2 - Indicates SAP Outdoor Channels operation only.

  RETURN VALUE
  The VOS_STATUS code associated with performing the operation

  VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_set_Dfs_Preferred_Channel_location(tHalHandle hHal,
                                   v_U8_t dfs_Preferred_Channels_location)
{
    tpAniSirGlobal pMac = NULL;
    VOS_STATUS status;
    uint8_t dfs_region;

    if (NULL != hHal)
    {
        pMac = PMAC_STRUCT( hHal );
    }
    else
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                  "%s: Invalid hHal pointer", __func__);
        return VOS_STATUS_E_FAULT;
    }

    vos_nv_get_dfs_region(&dfs_region);

    /*
     * The Indoor/Outdoor only random channel selection
     * restriction is currently enforeced only for
     * JAPAN regulatory domain.
     */
    if (DFS_MKK4_DOMAIN == dfs_region)
    {
        pMac->sap.SapDfsInfo.sap_operating_chan_preferred_location =
                                               dfs_Preferred_Channels_location;
        VOS_TRACE(VOS_MODULE_ID_SAP,
                  VOS_TRACE_LEVEL_INFO_LOW,
                  FL("sapdfs:Set Preferred Operating Channel location=%d"),
                  pMac->sap.SapDfsInfo.sap_operating_chan_preferred_location);

        status = VOS_STATUS_SUCCESS;
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
           FL("sapdfs:NOT JAPAN REG, Invalid Set preferred chans location"));

        status = VOS_STATUS_E_FAULT;
    }

    return status;
}

/*==========================================================================
  FUNCTION    WLANSAP_Set_Dfs_Target_Chnl

  DESCRIPTION
   This API is used to set next target chnl as provided channel.
   you can provide any valid channel to this API.

  DEPENDENCIES
   NA.

  PARAMETERS
  IN
  hHal : HAL pointer
  target_channel : target channel to be set

  RETURN VALUE
  The VOS_STATUS code associated with performing the operation

  VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS WLANSAP_Set_Dfs_Target_Chnl(tHalHandle hHal, v_U8_t target_channel)
{
    tpAniSirGlobal pMac = NULL;

    if (NULL != hHal)
    {
        pMac = PMAC_STRUCT( hHal );
    }
    else
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                  "%s: Invalid hHal pointer", __func__);
        return VOS_STATUS_E_FAULT;
    }
    if (target_channel > 0)
    {
        pMac->sap.SapDfsInfo.user_provided_target_channel = target_channel;
    }
    else
    {
        pMac->sap.SapDfsInfo.user_provided_target_channel = 0;
    }

    return VOS_STATUS_SUCCESS;
}

VOS_STATUS
WLANSAP_UpdateSapConfigAddIE(tsap_Config_t *pConfig,
                         const tANI_U8 *pAdditionIEBuffer,
                         tANI_U16 additionIELength,
                         eUpdateIEsType updateType)
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    tANI_U8  bufferValid = VOS_FALSE;
    tANI_U16 bufferLength = 0;
    tANI_U8 *pBuffer = NULL;

    if (NULL == pConfig)
    {
        return VOS_STATUS_E_FAULT;
    }

    if ( (pAdditionIEBuffer != NULL) && (additionIELength != 0) )
    {
           /* initialize the buffer pointer so that pe can copy*/
       if (additionIELength > 0)
       {
           bufferLength = additionIELength;
           pBuffer = vos_mem_malloc(bufferLength);
           if (NULL == pBuffer)
           {
                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                    FL("Could not allocate the buffer "));
                return VOS_STATUS_E_NOMEM;
            }
            vos_mem_copy(pBuffer, pAdditionIEBuffer, bufferLength);
            bufferValid = VOS_TRUE;
       }
    }

    switch(updateType)
    {
    case eUPDATE_IE_PROBE_BCN:
        if (bufferValid)
        {
            pConfig->probeRespBcnIEsLen = bufferLength;
            pConfig->pProbeRespBcnIEsBuffer = pBuffer;
        }
        else
        {
            vos_mem_free(pConfig->pProbeRespBcnIEsBuffer);
            pConfig->probeRespBcnIEsLen = 0;
            pConfig->pProbeRespBcnIEsBuffer = NULL;
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                FL("No Probe Resp beacone IE received in set beacon"));
        }
        break;
    case eUPDATE_IE_PROBE_RESP:
        if (bufferValid)
        {
            pConfig->probeRespIEsBufferLen= bufferLength;
            pConfig->pProbeRespIEsBuffer = pBuffer;
        }
        else
        {
            vos_mem_free(pConfig->pProbeRespIEsBuffer);
            pConfig->probeRespIEsBufferLen = 0;
            pConfig->pProbeRespIEsBuffer = NULL;
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                FL("No Probe Response IE received in set beacon"));
        }
        break;
    case eUPDATE_IE_ASSOC_RESP:
        if (bufferValid)
        {
            pConfig->assocRespIEsLen = bufferLength;
            pConfig->pAssocRespIEsBuffer = pBuffer;
        }
        else
        {
            vos_mem_free(pConfig->pAssocRespIEsBuffer);
            pConfig->assocRespIEsLen = 0;
            pConfig->pAssocRespIEsBuffer = NULL;
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                FL("No Assoc Response IE received in set beacon"));
        }
        break;
    default:
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                FL("No matching buffer type %d"), updateType);
            if (pBuffer != NULL)
                vos_mem_free(pBuffer);
        break;
    }

    return (status);
}


VOS_STATUS
WLANSAP_ResetSapConfigAddIE(tsap_Config_t *pConfig,
                            eUpdateIEsType updateType)
{
    if (NULL == pConfig) {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid Config pointer", __func__);
        return VOS_STATUS_E_FAULT;
    }

    switch (updateType)
    {
    case eUPDATE_IE_ALL:  /*only used to reset*/
    case eUPDATE_IE_PROBE_RESP:
        vos_mem_free( pConfig->pProbeRespIEsBuffer);
        pConfig->probeRespIEsBufferLen = 0;
        pConfig->pProbeRespIEsBuffer = NULL;
        if(eUPDATE_IE_ALL != updateType)  break;

    case eUPDATE_IE_ASSOC_RESP:
        vos_mem_free( pConfig->pAssocRespIEsBuffer);
        pConfig->assocRespIEsLen = 0;
        pConfig->pAssocRespIEsBuffer = NULL;
        if(eUPDATE_IE_ALL != updateType)  break;

    case eUPDATE_IE_PROBE_BCN:
        vos_mem_free(pConfig->pProbeRespBcnIEsBuffer );
        pConfig->probeRespBcnIEsLen = 0;
        pConfig->pProbeRespBcnIEsBuffer = NULL;
        if(eUPDATE_IE_ALL != updateType)  break;

    default:
        if(eUPDATE_IE_ALL != updateType)
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   FL("Invalid buffer type %d"), updateType);
        break;
    }
    return VOS_STATUS_SUCCESS;
}

/*==========================================================================
FUNCTION  WLANSAP_extend_to_acs_range

DESCRIPTION Function extends give channel range to consider ACS chan bonding

DEPENDENCIES PARAMETERS

IN /OUT
*startChannelNum : ACS extend start ch
*endChannelNum   : ACS extended End ch
*bandStartChannel: Band start ch
*bandEndChannel  : Band end ch

RETURN VALUE NONE

SIDE EFFECTS
============================================================================*/
v_VOID_t WLANSAP_extend_to_acs_range(v_U8_t *startChannelNum,
                                  v_U8_t *endChannelNum,
                                  v_U8_t *bandStartChannel,
                                  v_U8_t *bandEndChannel)
{
#define ACS_WLAN_20M_CH_INC 4
#define ACS_2G_EXTEND ACS_WLAN_20M_CH_INC
#define ACS_5G_EXTEND (ACS_WLAN_20M_CH_INC * 3)

    v_U8_t tmp_startChannelNum = 0, tmp_endChannelNum = 0;

    if (*startChannelNum <= 14) {
        *bandStartChannel = RF_CHAN_1;
        *bandEndChannel = RF_CHAN_14;
        tmp_startChannelNum = *startChannelNum > 5 ?
                               (*startChannelNum - ACS_2G_EXTEND): 1;
        tmp_endChannelNum = (*endChannelNum + ACS_2G_EXTEND) <= 14 ?
                               (*endChannelNum + ACS_2G_EXTEND):14;
    } else {
        *bandStartChannel = RF_CHAN_36;
        *bandEndChannel = RF_CHAN_165;
        tmp_startChannelNum = (*startChannelNum - ACS_5G_EXTEND) > 36 ?
                                (*startChannelNum - ACS_5G_EXTEND):36;
        tmp_endChannelNum = (*endChannelNum + ACS_5G_EXTEND) <= 165 ?
                             (*endChannelNum + ACS_5G_EXTEND):165;
    }

    /* Note if the ACS range include only DFS channels, do not cross the range.
     * Active scanning in adjacent non DFS channels results in transmission
     * spikes in DFS specturm channels which is due to emission spill.
     * Remove the active channels from extend ACS range for DFS only range
     */
    if (VOS_IS_DFS_CH(*startChannelNum)) {
        while (!VOS_IS_DFS_CH(tmp_startChannelNum) && tmp_startChannelNum <
                                                          *startChannelNum)
            tmp_startChannelNum += ACS_WLAN_20M_CH_INC;

        *startChannelNum = tmp_startChannelNum;
    }
    if (VOS_IS_DFS_CH(*endChannelNum)) {
        while (!VOS_IS_DFS_CH(tmp_endChannelNum) && tmp_endChannelNum >
                                                          *endChannelNum)
            tmp_endChannelNum -= ACS_WLAN_20M_CH_INC;

        *endChannelNum = tmp_endChannelNum;
    }
}

/*==========================================================================
  FUNCTION    WLANSAP_Get_DfsNol

  DESCRIPTION
  This API is used to dump the dfs nol
  DEPENDENCIES
  NA.

  PARAMETERS
  IN
  sapContext: Pointer to vos global context structure

  RETURN VALUE
  The VOS_STATUS code associated with performing the operation

  VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_Get_DfsNol(v_PVOID_t pSapCtx)
{
    int i = 0;
    ptSapContext sapContext = (ptSapContext)pSapCtx;
    v_PVOID_t hHal = NULL;
    tpAniSirGlobal pMac = NULL;
    v_U64_t current_time, found_time, elapsed_time;
    unsigned long left_time;
    tSapDfsNolInfo *dfs_nol = NULL;
    v_BOOL_t bAvailable = FALSE;

    if (NULL == sapContext)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                "%s: Invalid SAP pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }
    hHal = VOS_GET_HAL_CB(sapContext->pvosGCtx);
    if (NULL == hHal)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid HAL pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }
    pMac = PMAC_STRUCT( hHal );

    if (!pMac->sap.SapDfsInfo.numCurrentRegDomainDfsChannels) {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                "%s: DFS NOL is empty", __func__);
        return VOS_STATUS_SUCCESS;
    }

    dfs_nol = pMac->sap.SapDfsInfo.sapDfsChannelNolList;

    if (!dfs_nol) {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                "%s: DFS NOL context is null", __func__);
        return VOS_STATUS_E_FAULT;
    }

    for (i = 0; i < pMac->sap.SapDfsInfo.numCurrentRegDomainDfsChannels; i++)
    {
        if (!dfs_nol[i].dfs_channel_number)
            continue;

        current_time = vos_get_monotonic_boottime();
        found_time = dfs_nol[i].radar_found_timestamp;

        elapsed_time = current_time - found_time;

        /* check if channel is available
         * if either channel is usable or available, or timer expired 30mins
         */
        bAvailable =
            ((dfs_nol[i].radar_status_flag == eSAP_DFS_CHANNEL_AVAILABLE) ||
             (dfs_nol[i].radar_status_flag == eSAP_DFS_CHANNEL_USABLE)    ||
             (elapsed_time >= SAP_DFS_NON_OCCUPANCY_PERIOD));

        if (bAvailable)
        {
            dfs_nol[i].radar_status_flag = eSAP_DFS_CHANNEL_AVAILABLE;
            dfs_nol[i].radar_found_timestamp  = 0;

            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                "%s: Channel[%d] is AVAILABLE",
                __func__,
                dfs_nol[i].dfs_channel_number);
        } else {

            /* the time left in min */
            left_time = SAP_DFS_NON_OCCUPANCY_PERIOD - elapsed_time;
            left_time = left_time / (60 * 1000 * 1000);

            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                "%s: Channel[%d] is UNAVAILABLE [%lu min left]",
                __func__,
                dfs_nol[i].dfs_channel_number,
                left_time);
        }
    }

    return VOS_STATUS_SUCCESS;
}

/*==========================================================================
  FUNCTION    WLANSAP_Set_DfsNol

  DESCRIPTION
  This API is used to set the dfs nol
  DEPENDENCIES
  NA.

  PARAMETERS
  IN
  sapContext: Pointer to vos global context structure
  conf: set type

  RETURN VALUE
  The VOS_STATUS code associated with performing the operation

  VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_Set_DfsNol(v_PVOID_t pSapCtx, eSapDfsNolType conf)
{
    int i = 0;
    ptSapContext sapContext = (ptSapContext)pSapCtx;
    v_PVOID_t hHal = NULL;
    tpAniSirGlobal pMac = NULL;

    if (NULL == sapContext)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                "%s: Invalid SAP pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }
    hHal = VOS_GET_HAL_CB(sapContext->pvosGCtx);
    if (NULL == hHal)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid HAL pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }
    pMac = PMAC_STRUCT( hHal );

    if (!pMac->sap.SapDfsInfo.numCurrentRegDomainDfsChannels) {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                "%s: DFS NOL is empty", __func__);
        return VOS_STATUS_SUCCESS;
    }

    if (conf == eSAP_DFS_NOL_CLEAR) {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                "%s: clear the DFS NOL",
                __func__);

        for (i = 0; i < pMac->sap.SapDfsInfo.numCurrentRegDomainDfsChannels;
                i++)
        {
            if (!pMac->sap.SapDfsInfo.sapDfsChannelNolList[i].
                    dfs_channel_number)
                continue;

            pMac->sap.SapDfsInfo.sapDfsChannelNolList[i].
                radar_status_flag = eSAP_DFS_CHANNEL_AVAILABLE;
            pMac->sap.SapDfsInfo.sapDfsChannelNolList[i].
                radar_found_timestamp = 0;
        }
    } else if (conf == eSAP_DFS_NOL_RANDOMIZE) {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                "%s: Randomize the DFS NOL",
                __func__);

        /* random 1/0 to decide to put the channel into NOL */
        for (i = 0; i < pMac->sap.SapDfsInfo.numCurrentRegDomainDfsChannels;
                i++)
        {
            v_U32_t random_bytes = 0;
            get_random_bytes(&random_bytes, 1);

            if (!pMac->sap.SapDfsInfo.sapDfsChannelNolList[i].
                    dfs_channel_number)
                continue;

            if ((random_bytes + jiffies) % 2) {
                /* mark the channel unavailable */
                pMac->sap.SapDfsInfo.sapDfsChannelNolList[i]
                    .radar_status_flag = eSAP_DFS_CHANNEL_UNAVAILABLE;

                /* mark the timestamp */
                pMac->sap.SapDfsInfo.sapDfsChannelNolList[i]
                    .radar_found_timestamp = vos_get_monotonic_boottime();
            } else {
                /* mark the channel available */
                pMac->sap.SapDfsInfo.sapDfsChannelNolList[i].
                    radar_status_flag = eSAP_DFS_CHANNEL_AVAILABLE;

                /* clear the timestamp */
                pMac->sap.SapDfsInfo.sapDfsChannelNolList[i].
                    radar_found_timestamp = 0;
            }

            VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                        "%s: Set channel[%d] %s",
                        __func__,
                        pMac->sap.SapDfsInfo.sapDfsChannelNolList[i]
                            .dfs_channel_number,
                        (pMac->sap.SapDfsInfo.sapDfsChannelNolList[i].
                            radar_status_flag > eSAP_DFS_CHANNEL_AVAILABLE) ?
                                "UNAVAILABLE" : "AVAILABLE");
        }
    } else {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                "%s: unsupport type %d",
                __func__, conf);
    }

    /* set DFS-NOL back to keep it update-to-date in CNSS */
    sapSignalHDDevent(sapContext, NULL, eSAP_DFS_NOL_SET,
        (v_PVOID_t) eSAP_STATUS_SUCCESS);

    return VOS_STATUS_SUCCESS;
}

/*==========================================================================
  FUNCTION    WLANSAP_PopulateDelStaParams

  DESCRIPTION
  This API is used to populate del station parameters
  DEPENDENCIES
  NA.

  PARAMETERS
  IN
  mac:           pointer to peer mac address.
  reason_code:   Reason code for the disassoc/deauth.
  subtype:       subtype points to either disassoc/deauth frame.
  pDelStaParams: address where parameters to be populated.

  RETURN VALUE NONE

  SIDE EFFECTS
============================================================================*/
void WLANSAP_PopulateDelStaParams(const v_U8_t *mac,
                                  v_U16_t reason_code,
                                  v_U8_t subtype,
                                  struct tagCsrDelStaParams *pDelStaParams)
{
        if (NULL == mac)
            memset(pDelStaParams->peerMacAddr, 0xff, VOS_MAC_ADDR_SIZE);
        else
            vos_mem_copy(pDelStaParams->peerMacAddr, mac, VOS_MAC_ADDR_SIZE);

        if (reason_code == 0)
            pDelStaParams->reason_code = eSIR_MAC_DEAUTH_LEAVING_BSS_REASON;
        else
            pDelStaParams->reason_code = reason_code;

        if (subtype == (SIR_MAC_MGMT_DEAUTH >> 4) ||
            subtype == (SIR_MAC_MGMT_DISASSOC >> 4))
            pDelStaParams->subtype = subtype;
        else
            pDelStaParams->subtype = (SIR_MAC_MGMT_DEAUTH >> 4);

        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
               FL("Delete STA with RC:%hu subtype:%hhu MAC::" MAC_ADDRESS_STR),
                   pDelStaParams->reason_code, pDelStaParams->subtype,
                   MAC_ADDR_ARRAY(pDelStaParams->peerMacAddr));
}

/*==========================================================================
  FUNCTION    WLANSAP_ACS_CHSelect

  DESCRIPTION
    This api function provides ACS selection for BSS

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
      pvosGCtx: Pointer to vos global context structure
      pConfig: Pointer to configuration structure passed down from HDD
      pACSEventCallback: Callback function in HDD called by SAP to inform
                         HDD about channel section result
      usrDataForCallback: Parameter that will be passed back in all the
                          SAP callback events.

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_ACS_CHSelect(v_PVOID_t pvosGCtx,
                     tpWLAN_SAPEventCB pACSEventCallback,
                     tsap_Config_t *pConfig,
                     v_PVOID_t  pUsrContext)
{
    ptSapContext sapContext = NULL;
    tHalHandle hHal = NULL;
    VOS_STATUS vosStatus = VOS_STATUS_E_FAILURE;
    tpAniSirGlobal pMac = NULL;

    sapContext = VOS_GET_SAP_CB( pvosGCtx );
    if (NULL == sapContext) {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);

        return VOS_STATUS_E_FAULT;
    }

    hHal = (tHalHandle)VOS_GET_HAL_CB(sapContext->pvosGCtx);
    if (NULL == hHal) {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid MAC context from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    if (sapContext->isSapSessionOpen == eSAP_TRUE) {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_FATAL,
                   "%s:SME Session is already opened\n",__func__);
        return VOS_STATUS_E_EXISTS;
    }

    sapContext->sessionId = 0xff;

    pMac = PMAC_STRUCT( hHal );
    sapContext->acs_cfg = &pConfig->acs_cfg;
    sapContext->csrRoamProfile.phyMode = sapContext->acs_cfg->hw_mode;

    if (sapContext->isScanSessionOpen == eSAP_FALSE) {
        tANI_U32 type, subType;

        if(VOS_STATUS_SUCCESS ==
                      vos_get_vdev_types(VOS_STA_MODE, &type, &subType)) {
            /*
             * Open SME Session for scan
             */
            if(eHAL_STATUS_SUCCESS  != sme_OpenSession(hHal, NULL, sapContext,
                                                   sapContext->self_mac_addr,
                                                   &sapContext->sessionId,
                                                   type, subType)) {
                VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                           "Error: In %s calling sme_OpenSession", __func__);
                return VOS_STATUS_E_FAILURE;
            }
            else
                sapContext->isScanSessionOpen = eSAP_TRUE;
        }

        /*
         * Copy the HDD callback function to report the
         * ACS result after scan in SAP context callback function.
         */
        sapContext->pfnSapEventCallback = pACSEventCallback;
        /*
         * init dfs channel nol
         */
        sapInitDfsChannelNolList(sapContext);

        /*
         * Now, configure the scan and ACS channel params
         * to issue a scan request.
         */
        WLANSAP_SetScanAcsChannelParams(pConfig, sapContext, pUsrContext);

        /*
         * Issue the scan request. This scan request is
         * issued before the start BSS is done so
         *
         * 1. No need to pass the second parameter
         * as the SAP state machine is not started yet
         * and there is no need for any event posting.
         *
         * 2. Set third parameter to TRUE to indicate the
         * channel selection function to register a
         * different scan callback fucntion to process
         * the results pre start BSS.
         */
        vosStatus = sapGotoChannelSel(sapContext, NULL, VOS_TRUE);

        if (VOS_STATUS_E_ABORTED == vosStatus) {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "In %s,DFS not supported in the current operating mode",
                        __func__);
            return VOS_STATUS_E_FAILURE;
        }
        else if (VOS_STATUS_E_CANCELED == vosStatus) {
             /*
              * ERROR is returned when either the SME scan request
              * failed or ACS is overridden due to other constraints
              * So send this channel to HDD.
              */
             VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                 FL("Scan Req Failed/ACS Overridden, Selected channel = %d"),
                 sapContext->channel);

             if (sapContext->isScanSessionOpen == eSAP_TRUE) {
                 /* acs scan not needed so close the session */
                 tHalHandle  hHal = VOS_GET_HAL_CB(sapContext->pvosGCtx);
                 if (hHal == NULL) {
                     VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                         "%s: HAL Handle NULL. ACS Scan session close fail!",
                         __func__);
                     return VOS_STATUS_E_FAILURE;
                 }
                 if (eHAL_STATUS_SUCCESS == sme_CloseSession(hHal,
                                      sapContext->sessionId, NULL, NULL)) {
                         sapContext->isScanSessionOpen = eSAP_FALSE;
                 } else {
                     VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                         "%s: ACS Scan session close fail!", __func__);
                 }
                 sapContext->sessionId = 0xff;
             }

             return sapSignalHDDevent(sapContext, NULL,
                     eSAP_ACS_CHANNEL_SELECTED, (v_PVOID_t) eSAP_STATUS_SUCCESS);
        }
        else if (VOS_STATUS_SUCCESS == vosStatus)
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                       FL("Successfully Issued a Pre Start Bss Scan Request"));
    }
    return vosStatus;
}
/**
* wlansap_get_phymode() - get SAP phymode.
* @pctx: Pointer to the global vos context; a handle to SAP's control block
*        can be extracted from its context. When MBSSID feature is enabled,
*        SAP context is directly passed to SAP APIs.
*
* This function provides current phymode of SAP interface.
*
* Return: phymode with eCsrPhyMode type.
*/
eCsrPhyMode
wlansap_get_phymode(v_PVOID_t pctx)
{
	ptSapContext psapctx = VOS_GET_SAP_CB(pctx);

	if ( NULL == psapctx) {
		VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
			"%s: Invalid SAP pointer from pCtx", __func__);
		return eCSR_DOT11_MODE_AUTO;
	}
	return psapctx->csrRoamProfile.phyMode;
}

/**
 * wlansap_set_tx_leakage_threshold() - set sap tx leakage threshold.
 * @hal: HAL pointer
 * @tx_leakage_threshold: sap tx leakage threshold
 *
 * This function set sap tx leakage threshold.
 *
 * Return: VOS_STATUS.
 */
VOS_STATUS wlansap_set_tx_leakage_threshold(tHalHandle hal,
			uint16 tx_leakage_threshold)
{
	tpAniSirGlobal mac;

	if (NULL == hal) {
		VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
			"%s: Invalid hal pointer", __func__);
		return VOS_STATUS_E_FAULT;
	}

	mac = PMAC_STRUCT(hal);
	mac->sap.SapDfsInfo.tx_leakage_threshold = tx_leakage_threshold;
	VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
			"%s: leakage_threshold %d", __func__,
			mac->sap.SapDfsInfo.tx_leakage_threshold);
	return VOS_STATUS_SUCCESS;
}

