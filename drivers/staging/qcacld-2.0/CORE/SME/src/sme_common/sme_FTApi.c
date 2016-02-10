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

#ifdef WLAN_FEATURE_VOWIFI_11R
/**=========================================================================

  \brief Definitions for SME FT APIs

  ========================================================================*/

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <smsDebug.h>
#include <csrInsideApi.h>
#include <csrNeighborRoam.h>

/*--------------------------------------------------------------------------
  Initialize the FT context.
  ------------------------------------------------------------------------*/
void sme_FTOpen(tHalHandle hHal, tANI_U32 sessionId)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   eHalStatus     status = eHAL_STATUS_SUCCESS;
   tCsrRoamSession *pSession = CSR_GET_SESSION(pMac, sessionId);

   if (NULL != pSession) {
      /* Clean up the context */
      vos_mem_set(&pSession->ftSmeContext, sizeof(tftSMEContext), 0);

      pSession->ftSmeContext.pUsrCtx = vos_mem_malloc(
                                         sizeof(tFTRoamCallbackUsrCtx));

      if (NULL == pSession->ftSmeContext.pUsrCtx) {
          smsLog(pMac, LOGE, FL("Memory allocation failure"));
          return;
      }
      pSession->ftSmeContext.pUsrCtx->pMac = pMac;
      pSession->ftSmeContext.pUsrCtx->sessionId = sessionId;

      status =
         vos_timer_init(&pSession->ftSmeContext.preAuthReassocIntvlTimer,
         VOS_TIMER_TYPE_SW,
         sme_PreauthReassocIntvlTimerCallback,
         (void *)pSession->ftSmeContext.pUsrCtx);

      if (eHAL_STATUS_SUCCESS != status) {
         smsLog(pMac, LOGE,
               FL("Preauth Reassoc interval Timer allocation failed"));
         vos_mem_free(pSession->ftSmeContext.pUsrCtx);
         pSession->ftSmeContext.pUsrCtx = NULL;
         return;
      }
   }
}

/*--------------------------------------------------------------------------
  Cleanup the SME FT Global context.
  ------------------------------------------------------------------------*/
void sme_FTClose(tHalHandle hHal, tANI_U32 sessionId)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   tCsrRoamSession *pSession = NULL;

   /* Clear the FT Context */
   sme_FTReset(hHal, sessionId);

   pSession = CSR_GET_SESSION( pMac, sessionId );
   if (NULL != pSession)
   {
      /* check if the timer is running */
      if (VOS_TIMER_STATE_RUNNING ==
            vos_timer_getCurrentState(
                     &pSession->ftSmeContext.preAuthReassocIntvlTimer))
      {
         vos_timer_stop(&pSession->ftSmeContext.preAuthReassocIntvlTimer);
      }

      if (VOS_STATUS_SUCCESS !=
            vos_timer_destroy(&pSession->ftSmeContext.preAuthReassocIntvlTimer))
      {
         smsLog(pMac, LOGE, FL("preAuthReAssocTimer destroy failed"));
      }

      if (pSession->ftSmeContext.pUsrCtx != NULL) {
          smsLog(pMac, LOG1,
                 FL("Freeing ftSmeContext.pUsrCtx and setting to NULL"));
          vos_mem_free(pSession->ftSmeContext.pUsrCtx);
          pSession->ftSmeContext.pUsrCtx = NULL;
      }
   }
}

void sme_SetFTPreAuthState(tHalHandle hHal, tANI_U32 sessionId, v_BOOL_t state)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
   if (pSession)
      pSession->ftSmeContext.setFTPreAuthState = state;
}

v_BOOL_t sme_GetFTPreAuthState(tHalHandle hHal, tANI_U32 sessionId)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
   if (pSession)
      return pSession->ftSmeContext.setFTPreAuthState;

   return FALSE;
}

/*--------------------------------------------------------------------------
  Each time the supplicant sends down the FT IEs to the driver.
  This function is called in SME. This function packages and sends
  the FT IEs to PE.
  ------------------------------------------------------------------------*/
void sme_SetFTIEs(tHalHandle hHal, tANI_U32 sessionId, const tANI_U8 *ft_ies,
                 tANI_U16 ft_ies_length )
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
   eHalStatus status = eHAL_STATUS_FAILURE;

   if (NULL == pSession || NULL == ft_ies)
   {
      smsLog( pMac, LOGE, FL(" ft ies or pSession is NULL"));
      return;
   }

   status = sme_AcquireGlobalLock( &pMac->sme );
   if (!( HAL_STATUS_SUCCESS( status ))) return;

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
   smsLog( pMac, LOG1, "FT IEs Req is received in state %d",
         pSession->ftSmeContext.FTState);
#endif

   // Global Station FT State
   switch(pSession->ftSmeContext.FTState)
   {
      case eFT_START_READY:
      case eFT_AUTH_REQ_READY:
         if ((pSession->ftSmeContext.auth_ft_ies) &&
               (pSession->ftSmeContext.auth_ft_ies_length))
         {
            // Free the one we received last from the supplicant
            vos_mem_free(pSession->ftSmeContext.auth_ft_ies);
            pSession->ftSmeContext.auth_ft_ies_length = 0;
            pSession->ftSmeContext.auth_ft_ies = NULL;
         }

         // Save the FT IEs
         pSession->ftSmeContext.auth_ft_ies =
            vos_mem_malloc(ft_ies_length);
         if ( NULL == pSession->ftSmeContext.auth_ft_ies )
         {
            smsLog( pMac, LOGE, FL("Memory allocation failed for "
                     "auth_ft_ies"));
            sme_ReleaseGlobalLock( &pMac->sme );
            return;
         }
         pSession->ftSmeContext.auth_ft_ies_length = ft_ies_length;
         vos_mem_copy((tANI_U8 *)pSession->ftSmeContext.auth_ft_ies,
               ft_ies,ft_ies_length);
         pSession->ftSmeContext.FTState = eFT_AUTH_REQ_READY;

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
         smsLog( pMac, LOG1, "ft_ies_length=%d", ft_ies_length);
#endif
         break;

      case eFT_AUTH_COMPLETE:
         // We will need to re-start preauth. If we received FT IEs in
         // eFT_PRE_AUTH_DONE state, it implies there was a rekey in
         // our pre-auth state. Hence this implies we need Pre-auth again.

         // OK now inform SME we have no pre-auth list.
         // Delete the pre-auth node locally. Set your self back to restart pre-auth
         // TBD
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
         smsLog( pMac, LOG1,
               "Pre-auth done and now receiving---> AUTH REQ <---- in state %d",
               pSession->ftSmeContext.FTState);
         smsLog( pMac, LOG1, "Unhandled reception of FT IES in state %d",
               pSession->ftSmeContext.FTState);
#endif
         break;

      case eFT_REASSOC_REQ_WAIT:
         // We are done with pre-auth, hence now waiting for
         // reassoc req. This is the new FT Roaming in place

         // At this juncture we are ready to start sending Re-Assoc Req.
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
         smsLog( pMac, LOG1, "New Reassoc Req=%p in state %d",
               ft_ies, pSession->ftSmeContext.FTState);
#endif
         if ((pSession->ftSmeContext.reassoc_ft_ies) &&
               (pSession->ftSmeContext.reassoc_ft_ies_length))
         {
            // Free the one we received last from the supplicant
            vos_mem_free(pSession->ftSmeContext.reassoc_ft_ies);
            pSession->ftSmeContext.reassoc_ft_ies_length = 0;
         }

         // Save the FT IEs
         pSession->ftSmeContext.reassoc_ft_ies = vos_mem_malloc(ft_ies_length);
         if ( NULL == pSession->ftSmeContext.reassoc_ft_ies )
         {
            smsLog( pMac, LOGE, FL("Memory allocation failed for "
                     "reassoc_ft_ies"));
            sme_ReleaseGlobalLock( &pMac->sme );
            return;
         }
         pSession->ftSmeContext.reassoc_ft_ies_length = ft_ies_length;
         vos_mem_copy((tANI_U8 *)pSession->ftSmeContext.reassoc_ft_ies, ft_ies,
               ft_ies_length);

         pSession->ftSmeContext.FTState = eFT_SET_KEY_WAIT;
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
         smsLog( pMac, LOG1, "ft_ies_length=%d state=%d", ft_ies_length,
               pSession->ftSmeContext.FTState);
#endif

         break;

      default:
         smsLog( pMac, LOGE, FL(" Unhandled state=%d"),
               pSession->ftSmeContext.FTState);
         break;
   }
   sme_ReleaseGlobalLock( &pMac->sme );
}

eHalStatus sme_FTSendUpdateKeyInd(tHalHandle hHal, tANI_U32 sessionId,
                                 tCsrRoamSetKey * pFTKeyInfo)
{
   tSirFTUpdateKeyInfo *pMsg;
   tANI_U16 msgLen;
   eHalStatus status = eHAL_STATUS_FAILURE;
   tAniEdType tmpEdType;
   tSirKeyMaterial *keymaterial = NULL;
   tAniEdType edType;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
   int i = 0;

   smsLog(pMac, LOG1, FL("keyLength %d"), pFTKeyInfo->keyLength);

   for (i=0; i<pFTKeyInfo->keyLength; i++)
      smsLog(pMac, LOG1, FL("%02x"), pFTKeyInfo->Key[i]);
#endif

   if(pFTKeyInfo->keyLength > CSR_MAX_KEY_LEN)
   {
      smsLog( pMac, LOGE, "%s: invalid keyLength %d", __func__,pFTKeyInfo->keyLength);
      return eHAL_STATUS_FAILURE;
   }

   msgLen  = sizeof(tSirFTUpdateKeyInfo);

   pMsg = vos_mem_malloc(msgLen);
   if ( NULL == pMsg )
   {
      return eHAL_STATUS_FAILURE;
   }

   vos_mem_set(pMsg, msgLen, 0);
   pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_FT_UPDATE_KEY);
   pMsg->length = pal_cpu_to_be16(msgLen);

   keymaterial = &pMsg->keyMaterial;

   keymaterial->length = pFTKeyInfo->keyLength;

   edType = csrTranslateEncryptTypeToEdType( pFTKeyInfo->encType );
   tmpEdType = pal_cpu_to_be32(edType);
   keymaterial->edType = tmpEdType;

   // Set the pMsg->keyMaterial.length field (this length is defined as all
   // data that follows the edType field
   // in the tSirKeyMaterial keyMaterial; field).
   //
   // !!NOTE:  This keyMaterial.length contains the length of a MAX size key,
   // though the keyLength can be
   // shorter than this max size.  Is LIM interpreting this ok ?
   keymaterial->numKeys = 1;
   keymaterial->key[ 0 ].keyId = pFTKeyInfo->keyId;
   keymaterial->key[ 0 ].unicast = (tANI_U8)eANI_BOOLEAN_TRUE;
   keymaterial->key[ 0 ].keyDirection = pFTKeyInfo->keyDirection;

   vos_mem_copy(&keymaterial->key[ 0 ].keyRsc, pFTKeyInfo->keyRsc, CSR_MAX_RSC_LEN);
   keymaterial->key[ 0 ].paeRole = pFTKeyInfo->paeRole;

   keymaterial->key[ 0 ].keyLength = pFTKeyInfo->keyLength;

   if ( pFTKeyInfo->keyLength && pFTKeyInfo->Key )
   {
      vos_mem_copy(&keymaterial->key[ 0 ].key, pFTKeyInfo->Key, pFTKeyInfo->keyLength);
      if(pFTKeyInfo->keyLength == 16)
      {
         smsLog(pMac, LOG1,
         "SME Set Update Ind keyIdx (%d) encType(%d) key = "
         "%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X",
         pMsg->keyMaterial.key[0].keyId, (tAniEdType)pMsg->keyMaterial.edType,
         pMsg->keyMaterial.key[0].key[0], pMsg->keyMaterial.key[0].key[1],
         pMsg->keyMaterial.key[0].key[2], pMsg->keyMaterial.key[0].key[3],
         pMsg->keyMaterial.key[0].key[4], pMsg->keyMaterial.key[0].key[5],
         pMsg->keyMaterial.key[0].key[6], pMsg->keyMaterial.key[0].key[7],
         pMsg->keyMaterial.key[0].key[8], pMsg->keyMaterial.key[0].key[9],
         pMsg->keyMaterial.key[0].key[10], pMsg->keyMaterial.key[0].key[11],
         pMsg->keyMaterial.key[0].key[12], pMsg->keyMaterial.key[0].key[13],
         pMsg->keyMaterial.key[0].key[14], pMsg->keyMaterial.key[0].key[15]);
      }
   }

   vos_mem_copy( &pMsg->bssId[ 0 ],
         &pFTKeyInfo->peerMac[ 0 ],
         sizeof(tCsrBssid) );

   pMsg->smeSessionId = sessionId;

   smsLog(pMac, LOG1, "BSSID = "MAC_ADDRESS_STR,
         MAC_ADDR_ARRAY(pMsg->bssId));

   status = palSendMBMessage(pMac->hHdd, pMsg);

   return( status );
}

v_BOOL_t sme_GetFTPTKState(tHalHandle hHal, tANI_U32 sessionId)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   tCsrRoamSession *pSession = CSR_GET_SESSION(pMac, sessionId);

   if (!pSession) {
      smsLog(pMac, LOGE, FL("pSession is NULL"));
      return VOS_FALSE;
   }
   return pSession->ftSmeContext.setFTPTKState;
}

void sme_SetFTPTKState(tHalHandle hHal, tANI_U32 sessionId, v_BOOL_t state)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   tCsrRoamSession *pSession = CSR_GET_SESSION(pMac, sessionId);

   if (!pSession) {
      smsLog(pMac, LOGE, FL("pSession is NULL"));
      return;
   }
   pSession->ftSmeContext.setFTPTKState = state;
}

eHalStatus sme_FTUpdateKey(tHalHandle hHal, tANI_U32 sessionId,
                           tCsrRoamSetKey * pFTKeyInfo )
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
   eHalStatus status = eHAL_STATUS_FAILURE;

   if (!pSession)
   {
      smsLog( pMac, LOGE, FL("pSession is NULL"));
      return eHAL_STATUS_FAILURE;
   }

   if (pFTKeyInfo == NULL)
   {
      smsLog( pMac, LOGE, "%s: pFTKeyInfo is NULL", __func__);
      return eHAL_STATUS_FAILURE;
   }

   status = sme_AcquireGlobalLock( &pMac->sme );
   if (!( HAL_STATUS_SUCCESS( status )))
   {
      return eHAL_STATUS_FAILURE;
   }

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
   smsLog( pMac, LOG1, "sme_FTUpdateKey is received in state %d",
         pSession->ftSmeContext.FTState);
#endif

   // Global Station FT State
   switch(pSession->ftSmeContext.FTState)
   {
      case eFT_SET_KEY_WAIT:
         if (sme_GetFTPreAuthState(hHal, sessionId) == TRUE)
         {
            status = sme_FTSendUpdateKeyInd(pMac, sessionId, pFTKeyInfo);
            if (status != 0 )
            {
               smsLog( pMac, LOGE, "%s: Key set failure %d", __func__,
                     status);
               pSession->ftSmeContext.setFTPTKState = FALSE;
               status = eHAL_STATUS_FT_PREAUTH_KEY_FAILED;
            }
            else
            {
               pSession->ftSmeContext.setFTPTKState = TRUE;
               status = eHAL_STATUS_FT_PREAUTH_KEY_SUCCESS;
               smsLog( pMac, LOG1, "%s: Key set success", __func__);
            }
            sme_SetFTPreAuthState(hHal, sessionId, FALSE);
         }

         pSession->ftSmeContext.FTState = eFT_START_READY;
#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
         smsLog( pMac, LOG1, "%s: state changed to %d status %d", __func__,
               pSession->ftSmeContext.FTState, status);
#endif
         break;

      default:
         smsLog( pMac, LOGW, "%s: Unhandled state=%d", __func__,
               pSession->ftSmeContext.FTState);
         status = eHAL_STATUS_FAILURE;
         break;
   }
   sme_ReleaseGlobalLock( &pMac->sme );

   return status;
}
/*--------------------------------------------------------------------------
 *
 * HDD Interface to SME. SME now sends the Auth 2 and RIC IEs up to the supplicant.
 * The supplicant will then proceed to send down the
 * Reassoc Req.
 *
 *------------------------------------------------------------------------*/
void sme_GetFTPreAuthResponse( tHalHandle hHal, tANI_U32 sessionId, tANI_U8 *ft_ies,
                               tANI_U32 ft_ies_ip_len, tANI_U16 *ft_ies_length )
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
   eHalStatus status = eHAL_STATUS_FAILURE;

   if (!pSession)
   {
      smsLog( pMac, LOGE, FL("pSession is NULL"));
      return;
   }

   *ft_ies_length = 0;

   status = sme_AcquireGlobalLock( &pMac->sme );
   if (!( HAL_STATUS_SUCCESS( status )))
      return;

   /* All or nothing - proceed only if both BSSID and FT IE fit */
   if((ANI_MAC_ADDR_SIZE +
     pSession->ftSmeContext.psavedFTPreAuthRsp->ft_ies_length) > ft_ies_ip_len)
   {
      sme_ReleaseGlobalLock( &pMac->sme );
      return;
   }

   // hdd needs to pack the bssid also along with the
   // auth response to supplicant
   vos_mem_copy(ft_ies, pSession->ftSmeContext.preAuthbssId, ANI_MAC_ADDR_SIZE);

   // Copy the auth resp FTIEs
   vos_mem_copy(&(ft_ies[ANI_MAC_ADDR_SIZE]),
         pSession->ftSmeContext.psavedFTPreAuthRsp->ft_ies,
         pSession->ftSmeContext.psavedFTPreAuthRsp->ft_ies_length);

   *ft_ies_length = ANI_MAC_ADDR_SIZE +
      pSession->ftSmeContext.psavedFTPreAuthRsp->ft_ies_length;

   pSession->ftSmeContext.FTState = eFT_REASSOC_REQ_WAIT;

#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
   smsLog( pMac, LOG1, FL(" Filled auth resp = %d"), *ft_ies_length);
#endif
   sme_ReleaseGlobalLock( &pMac->sme );
   return;
}

/*--------------------------------------------------------------------------
 *
 * SME now sends the RIC IEs up to the supplicant.
 * The supplicant will then proceed to send down the
 * Reassoc Req.
 *
 *------------------------------------------------------------------------*/
void sme_GetRICIEs(tHalHandle hHal, tANI_U32 sessionId, tANI_U8 *ric_ies,
                  tANI_U32 ric_ies_ip_len, tANI_U32 *ric_ies_length )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    eHalStatus status = eHAL_STATUS_FAILURE;

    if (!pSession)
    {
       smsLog( pMac, LOGE, FL("pSession is NULL"));
       return;
    }

    *ric_ies_length = 0;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if (!( HAL_STATUS_SUCCESS( status )))
       return;

    /* All or nothing */
    if (pSession->ftSmeContext.psavedFTPreAuthRsp->ric_ies_length >
        ric_ies_ip_len)
    {
       sme_ReleaseGlobalLock( &pMac->sme );
       return;
    }

    vos_mem_copy(ric_ies, pSession->ftSmeContext.psavedFTPreAuthRsp->ric_ies,
                 pSession->ftSmeContext.psavedFTPreAuthRsp->ric_ies_length);

    *ric_ies_length = pSession->ftSmeContext.psavedFTPreAuthRsp->ric_ies_length;

#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
    smsLog( pMac, LOG1, FL(" Filled ric ies = %d"), *ric_ies_length);
#endif

    sme_ReleaseGlobalLock( &pMac->sme );
    return;
}

/*--------------------------------------------------------------------------
 *
 * Timer callback for the timer that is started between the preauth completion and
 * reassoc request to the PE. In this interval, it is expected that the pre-auth response
 * and RIC IEs are passed up to the WPA supplicant and received back the necessary FTIEs
 * required to be sent in the reassoc request
 *
 *------------------------------------------------------------------------*/
void sme_PreauthReassocIntvlTimerCallback(void *context)
{
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
    tFTRoamCallbackUsrCtx *pUsrCtx = (tFTRoamCallbackUsrCtx *)context;

    if (pUsrCtx) {
        csrNeighborRoamRequestHandoff(pUsrCtx->pMac, pUsrCtx->sessionId);
    }
#endif
    return;
}

/*--------------------------------------------------------------------------
  Reset the FT context.
  ------------------------------------------------------------------------*/
void sme_FTReset(tHalHandle hHal, tANI_U32 sessionId)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   tCsrRoamSession *pSession = NULL;

   if (pMac == NULL) {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, FL("pMac is NULL"));
      return;
   }

   pSession = CSR_GET_SESSION(pMac, sessionId);
   if (NULL != pSession) {
      if (pSession->ftSmeContext.auth_ft_ies != NULL) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
          smsLog(pMac, LOG1, FL("Freeing FT Auth IE %p and setting to NULL"),
                pSession->ftSmeContext.auth_ft_ies);
#endif
         vos_mem_free(pSession->ftSmeContext.auth_ft_ies);
         pSession->ftSmeContext.auth_ft_ies = NULL;
      }
      pSession->ftSmeContext.auth_ft_ies_length = 0;

      if (pSession->ftSmeContext.reassoc_ft_ies != NULL) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
          smsLog(pMac, LOG1,
                 FL("Freeing FT Reassoc IE %p and setting to NULL"),
                 pSession->ftSmeContext.reassoc_ft_ies);
#endif
         vos_mem_free(pSession->ftSmeContext.reassoc_ft_ies);
         pSession->ftSmeContext.reassoc_ft_ies = NULL;
      }
      pSession->ftSmeContext.reassoc_ft_ies_length = 0;

      if (pSession->ftSmeContext.psavedFTPreAuthRsp != NULL) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
          smsLog( pMac, LOG1, FL("Freeing FtPreAuthRsp %p and setting to NULL"),
                pSession->ftSmeContext.psavedFTPreAuthRsp);
#endif
          vos_mem_free(pSession->ftSmeContext.psavedFTPreAuthRsp);
          pSession->ftSmeContext.psavedFTPreAuthRsp = NULL;
      }
      pSession->ftSmeContext.setFTPreAuthState = VOS_FALSE;
      pSession->ftSmeContext.setFTPTKState = VOS_FALSE;

      vos_mem_zero(pSession->ftSmeContext.preAuthbssId, ANI_MAC_ADDR_SIZE);
      pSession->ftSmeContext.FTState = eFT_START_READY;
   }
}

/* End of File */
#endif /* WLAN_FEATURE_VOWIFI_11R */
