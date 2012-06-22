/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

/**=========================================================================
 *     
 *       \file  wlan_qct_wdi_dts.c
 *          
 *       \brief  Data Transport Service API 
 *                               
 * WLAN Device Abstraction layer External API for Dataservice
 * DESCRIPTION
 *  This file contains the external API implemntation exposed by the 
 *   wlan device abstarction layer module.
 *
 *   Copyright (c) 2008 QUALCOMM Incorporated. All Rights Reserved.
 *   Qualcomm Confidential and Proprietary
 */


#include "wlan_qct_wdi.h"
#include "wlan_qct_dxe.h"
#include "wlan_qct_wdi_ds.h"
#include "wlan_qct_wdi_ds_i.h"
#include "wlan_qct_wdi_dts.h"
#include "wlan_qct_wdi_dp.h"
#include "wlan_qct_wdi_sta.h"

static WDTS_TransportDriverTrype gTransportDriver = {
  WLANDXE_Open, 
  WLANDXE_Start, 
  WLANDXE_ClientRegistration, 
  WLANDXE_TxFrame,
  WLANDXE_CompleteTX,
  WLANDXE_SetPowerState,
  WLANDXE_Stop,
  WLANDXE_Close,
  WLANDXE_GetFreeTxDataResNumber
};

static WDTS_SetPowerStateCbInfoType gSetPowerStateCbInfo;

/* DTS Tx packet complete function. 
 * This function should be invoked by the transport device to indicate 
 * transmit complete for a frame.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller 
 * pFrame:Refernce to PAL frame.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_TxPacketComplete(void *pContext, wpt_packet *pFrame, wpt_status status)
{
  WDI_DS_ClientDataType *pClientData = (WDI_DS_ClientDataType*)(pContext);
  WDI_DS_TxMetaInfoType     *pTxMetadata;
  void *pvBDHeader, *physBDHeader;
  wpt_uint8 staIndex;

  // Do Sanity checks
  if(NULL == pContext || NULL == pFrame){
    return eWLAN_PAL_STATUS_E_FAILURE;
  }


  // extract metadata from PAL packet
  pTxMetadata = WDI_DS_ExtractTxMetaData(pFrame);
  pTxMetadata->txCompleteStatus = status;

  // Free BD header from pool
  WDI_GetBDPointers(pFrame, &pvBDHeader,  &physBDHeader);
  switch(pTxMetadata->frmType) 
  {
    case WDI_MAC_DATA_FRAME:
      /* SWAP BD header to get STA index for completed frame */
      WDI_SwapTxBd(pvBDHeader);
      staIndex = (wpt_uint8)WDI_TX_BD_GET_STA_ID(pvBDHeader);
      WDI_DS_MemPoolFree(&(pClientData->dataMemPool), pvBDHeader, physBDHeader);
      WDI_DS_MemPoolDecreaseReserveCount(&(pClientData->dataMemPool), staIndex);
      break;
    case WDI_MAC_MGMT_FRAME:
      WDI_DS_MemPoolFree(&(pClientData->mgmtMemPool), pvBDHeader, physBDHeader);
      break;
  }
  WDI_SetBDPointers(pFrame, 0, 0);

  // Invoke Tx complete callback
  pClientData->txCompleteCB(pClientData->pCallbackContext, pFrame);  
  return eWLAN_PAL_STATUS_SUCCESS;

}


/*===============================================================================
  FUNCTION      WLANTL_GetReplayCounterFromRxBD
     
  DESCRIPTION   This function extracts 48-bit replay packet number from RX BD 
 
  DEPENDENCIES  Validity of replay check must be done before the function 
                is called
                          
  PARAMETERS    pucRxHeader pointer to RX BD header
                                       
  RETRUN        v_U64_t    Packet number extarcted from RX BD

  SIDE EFFECTS   none
 ===============================================================================*/
v_U64_t
WDTS_GetReplayCounterFromRxBD
(
   v_U8_t *pucRxBDHeader
)
{
  v_U64_t ullcurrentReplayCounter = 0;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* 48-bit replay counter is created as follows
   from RX BD 6 byte PMI command:
   Addr : AES/TKIP
   0x38 : pn3/tsc3
   0x39 : pn2/tsc2
   0x3a : pn1/tsc1
   0x3b : pn0/tsc0

   0x3c : pn5/tsc5
   0x3d : pn4/tsc4 */
  
#ifdef ANI_BIG_BYTE_ENDIAN
    /* Getting 48-bit replay counter from the RX BD */
    ullcurrentReplayCounter = WDI_RX_BD_GET_PMICMD_20TO23(pucRxBDHeader); 
    ullcurrentReplayCounter <<= 16;
    ullcurrentReplayCounter |= (( WDI_RX_BD_GET_PMICMD_24TO25(pucRxBDHeader) & 0xFFFF0000) >> 16);
    return ullcurrentReplayCounter;
#else
    /* Getting 48-bit replay counter from the RX BD */
    ullcurrentReplayCounter = (WDI_RX_BD_GET_PMICMD_24TO25(pucRxBDHeader) & 0x0000FFFF); 
    ullcurrentReplayCounter <<= 32; 
    ullcurrentReplayCounter |= WDI_RX_BD_GET_PMICMD_20TO23(pucRxBDHeader); 
    return ullcurrentReplayCounter;
#endif
}


/* DTS Rx packet function. 
 * This function should be invoked by the transport device to indicate 
 * reception of a frame.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller 
 * pFrame:Refernce to PAL frame.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_RxPacket (void *pContext, wpt_packet *pFrame, WDTS_ChannelType channel)
{
  WDI_DS_ClientDataType *pClientData = 
    (WDI_DS_ClientDataType*)(pContext);
  wpt_boolean       bASF, bFSF, bLSF, bAEF;
  wpt_uint8                   ucMPDUHOffset, ucMPDUHLen, ucTid;
  wpt_uint8                   *pBDHeader;
  wpt_uint16                  usMPDUDOffset, usMPDULen;
  WDI_DS_RxMetaInfoType     *pRxMetadata;
  wpt_uint8                  isFcBd = 0;

  tpSirMacFrameCtl  pMacFrameCtl;
  // Do Sanity checks
  if(NULL == pContext || NULL == pFrame){
    return eWLAN_PAL_STATUS_E_FAILURE;
  }

  /*------------------------------------------------------------------------
    Extract BD header and check if valid
    ------------------------------------------------------------------------*/
  pBDHeader = (wpt_uint8*)wpalPacketGetRawBuf(pFrame);
  if(NULL == pBDHeader)
  {
    DTI_TRACE( DTI_TRACE_LEVEL_ERROR,
       "WLAN TL:BD header received NULL - dropping packet");
    wpalPacketFree(pFrame);
    return eWLAN_PAL_STATUS_E_FAILURE;
  }
  WDI_SwapRxBd(pBDHeader);

  ucMPDUHOffset = (wpt_uint8)WDI_RX_BD_GET_MPDU_H_OFFSET(pBDHeader);
  usMPDUDOffset = (wpt_uint16)WDI_RX_BD_GET_MPDU_D_OFFSET(pBDHeader);
  usMPDULen     = (wpt_uint16)WDI_RX_BD_GET_MPDU_LEN(pBDHeader);
  ucMPDUHLen    = (wpt_uint8)WDI_RX_BD_GET_MPDU_H_LEN(pBDHeader);
  ucTid         = (wpt_uint8)WDI_RX_BD_GET_TID(pBDHeader);

  /*------------------------------------------------------------------------
    Gather AMSDU information 
    ------------------------------------------------------------------------*/
  bASF = WDI_RX_BD_GET_ASF(pBDHeader);
  bAEF = WDI_RX_BD_GET_AEF(pBDHeader);
  bFSF = WDI_RX_BD_GET_ESF(pBDHeader);
  bLSF = WDI_RX_BD_GET_LSF(pBDHeader);
  isFcBd = WDI_RX_FC_BD_GET_FC(pBDHeader);

  DTI_TRACE( DTI_TRACE_LEVEL_INFO,
      "WLAN TL:BD header processing data: HO %d DO %d Len %d HLen %d"
      " Tid %d BD %d",
      ucMPDUHOffset, usMPDUDOffset, usMPDULen, ucMPDUHLen, ucTid,
      WDI_RX_BD_HEADER_SIZE);

  if(!isFcBd)
  {
      if(usMPDUDOffset <= ucMPDUHOffset || usMPDULen < ucMPDUHLen) {
        DTI_TRACE( DTI_TRACE_LEVEL_ERROR,
            "WLAN TL:BD header corrupted - dropping packet");
        /* Drop packet ???? */ 
        wpalPacketFree(pFrame);
        return eWLAN_PAL_STATUS_SUCCESS;
      }

      if((ucMPDUHOffset < WDI_RX_BD_HEADER_SIZE) &&  (!(bASF && !bFSF))){
        /* AMSDU case, ucMPDUHOffset = 0  it should be hancdled seperatly */
        /* Drop packet ???? */ 
        wpalPacketFree(pFrame);
        return eWLAN_PAL_STATUS_SUCCESS;
      }

      /* AMSDU frame, but not first sub-frame
       * No MPDU header, MPDU header offset is 0
       * Total frame size is actual frame size + MPDU data offset */
      if((ucMPDUHOffset < WDI_RX_BD_HEADER_SIZE) && (bASF && !bFSF)){
        ucMPDUHOffset = usMPDUDOffset;
      }

      if(VPKT_SIZE_BUFFER < (usMPDULen+ucMPDUHOffset)){
        DTI_TRACE( DTI_TRACE_LEVEL_FATAL,
                   "Invalid Frame size, might memory corrupted");
        wpalPacketFree(pFrame);
        return eWLAN_PAL_STATUS_SUCCESS;
      }
      wpalPacketSetRxLength(pFrame, usMPDULen+ucMPDUHOffset);
      wpalPacketRawTrimHead(pFrame, ucMPDUHOffset);

     

      pRxMetadata = WDI_DS_ExtractRxMetaData(pFrame);

      pRxMetadata->fc = isFcBd;
      pRxMetadata->staId = WDI_RX_BD_GET_STA_ID(pBDHeader);
      pRxMetadata->addr3Idx = WDI_RX_BD_GET_ADDR3_IDX(pBDHeader);
      pRxMetadata->rxChannel = WDI_RX_BD_GET_RX_CHANNEL(pBDHeader);
      pRxMetadata->rtsf = WDI_RX_BD_GET_RTSF(pBDHeader);
      pRxMetadata->bsf = WDI_RX_BD_GET_BSF(pBDHeader);
      pRxMetadata->scan = WDI_RX_BD_GET_SCAN(pBDHeader);
      pRxMetadata->dpuSig = WDI_RX_BD_GET_DPU_SIG(pBDHeader);
      pRxMetadata->ft = WDI_RX_BD_GET_FT(pBDHeader);
      pRxMetadata->ne = WDI_RX_BD_GET_NE(pBDHeader);
      pRxMetadata->llcr = WDI_RX_BD_GET_LLCR(pBDHeader);
      pRxMetadata->bcast = WDI_RX_BD_GET_UB(pBDHeader);
      pRxMetadata->tid = ucTid;
      pRxMetadata->dpuFeedback = WDI_RX_BD_GET_DPU_FEEDBACK(pBDHeader);
      pRxMetadata->rateIndex = WDI_RX_BD_GET_RATEINDEX(pBDHeader);
      pRxMetadata->rxpFlags = WDI_RX_BD_GET_RXPFLAGS(pBDHeader);
      pRxMetadata->mclkRxTimestamp = WDI_RX_BD_GET_TIMESTAMP(pBDHeader);

      /* typeSubtype in BD doesn't look like correct. Fill from frame ctrl
         TL does it for Volans but TL does not know BD for Prima. WDI should do it */
      if ( 0 == WDI_RX_BD_GET_FT(pBDHeader) ) {
        if ( bASF ) {
          pRxMetadata->subtype = WDI_MAC_DATA_QOS_DATA;
          pRxMetadata->type    = WDI_MAC_DATA_FRAME;
        } else {
          pMacFrameCtl = (tpSirMacFrameCtl)(((wpt_uint8*)pBDHeader) + ucMPDUHOffset);
          pRxMetadata->subtype = pMacFrameCtl->subType;
          pRxMetadata->type    = pMacFrameCtl->type;
        }
      } else {
        pMacFrameCtl = (tpSirMacFrameCtl)(((wpt_uint8*)pBDHeader) + WDI_RX_BD_HEADER_SIZE);
        pRxMetadata->subtype = pMacFrameCtl->subType;
        pRxMetadata->type    = pMacFrameCtl->type;
      }

      pRxMetadata->mpduHeaderPtr = pBDHeader + ucMPDUHOffset;
      pRxMetadata->mpduDataPtr = pBDHeader + usMPDUDOffset;
      pRxMetadata->mpduLength = usMPDULen;
      pRxMetadata->mpduHeaderLength = ucMPDUHLen;

      /*------------------------------------------------------------------------
        Gather AMPDU information 
        ------------------------------------------------------------------------*/
      pRxMetadata->ampdu_reorderOpcode  = (wpt_uint8)WDI_RX_BD_GET_BA_OPCODE(pBDHeader);
      pRxMetadata->ampdu_reorderSlotIdx = (wpt_uint8)WDI_RX_BD_GET_BA_SI(pBDHeader);
      pRxMetadata->ampdu_reorderFwdIdx  = (wpt_uint8)WDI_RX_BD_GET_BA_FI(pBDHeader);
      pRxMetadata->currentPktSeqNo       = (wpt_uint8)WDI_RX_BD_GET_BA_CSN(pBDHeader);


      /*------------------------------------------------------------------------
        Gather AMSDU information 
        ------------------------------------------------------------------------*/
      pRxMetadata->amsdu_asf  =  bASF;
      pRxMetadata->amsdu_aef  =  bAEF;
      pRxMetadata->amsdu_esf  =  bFSF;
      pRxMetadata->amsdu_lsf  =  bLSF;
      pRxMetadata->amsdu_size =  WDI_RX_BD_GET_AMSDU_SIZE(pBDHeader);

      pRxMetadata->rssi0 = WDI_RX_BD_GET_RSSI0(pBDHeader);
      pRxMetadata->rssi1 = WDI_RX_BD_GET_RSSI1(pBDHeader);


        /* Missing: 
      wpt_uint32 fcSTATxQStatus:8;
      wpt_uint32 fcSTAThreshIndMask:8;
      wpt_uint32 fcSTAPwrSaveStateMask:8;
      wpt_uint32 fcSTAValidMask:8;

      wpt_uint8 fcSTATxQLen[8]; // one byte per STA. 
      wpt_uint8 fcSTACurTxRate[8]; // current Tx rate for each sta.   
      unknownUcastPkt 
      */

      pRxMetadata->replayCount = WDTS_GetReplayCounterFromRxBD(pBDHeader);
      pRxMetadata->snr = WDI_RX_BD_GET_SNR(pBDHeader); 

      /* 
       * PAL BD pointer information needs to be populated 
       */ 
      WPAL_PACKET_SET_BD_POINTER(pFrame, pBDHeader);
      WPAL_PACKET_SET_BD_LENGTH(pFrame, sizeof(WDI_RxBdType));

      // Invoke Rx complete callback
      pClientData->receiveFrameCB(pClientData->pCallbackContext, pFrame);  
  }
  else
  {
      wpalPacketSetRxLength(pFrame, usMPDULen+ucMPDUHOffset);
      wpalPacketRawTrimHead(pFrame, ucMPDUHOffset);

      pRxMetadata = WDI_DS_ExtractRxMetaData(pFrame);
      //flow control related
      pRxMetadata->fc = isFcBd;
      pRxMetadata->fcStaTxDisabledBitmap = WDI_RX_FC_BD_GET_STA_TX_DISABLED_BITMAP(pBDHeader);
      pRxMetadata->fcSTAValidMask = WDI_RX_FC_BD_GET_STA_VALID_MASK(pBDHeader);
      // Invoke Rx complete callback
      pClientData->receiveFrameCB(pClientData->pCallbackContext, pFrame);  
  }
  return eWLAN_PAL_STATUS_SUCCESS;

}



/* DTS Out of Resource packet function. 
 * This function should be invoked by the transport device to indicate 
 * the device is out of resources.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller 
 * priority: indicates which channel is out of resource.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 */
wpt_status WDTS_OOResourceNotification(void *pContext, WDTS_ChannelType channel, wpt_boolean on)
{
  WDI_DS_ClientDataType *pClientData =
    (WDI_DS_ClientDataType *) pContext;
  static wpt_uint8 ac_mask = 0x1f;

  // Do Sanity checks
  if(NULL == pContext){
    return eWLAN_PAL_STATUS_E_FAILURE;
  }
  
  if(on){
    ac_mask |=  channel == WDTS_CHANNEL_TX_LOW_PRI?  0x0f : 0x10;
  } else {
    ac_mask &=  channel == WDTS_CHANNEL_TX_LOW_PRI?  0x10 : 0x0f;
  }


  // Invoke OOR callback
  pClientData->txResourceCB(pClientData->pCallbackContext, ac_mask); 
  return eWLAN_PAL_STATUS_SUCCESS;

}

/* DTS open  function. 
 * On open the transport device should initialize itself.
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along 
 *  with the callback.
 *
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_openTransport( void *pContext)
{
  void *pDTDriverContext; 
  WDI_DS_ClientDataType *pClientData;
  WDI_Status sWdiStatus = WDI_STATUS_SUCCESS;

  pClientData = (WDI_DS_ClientDataType*) wpalMemoryAllocate(sizeof(WDI_DS_ClientDataType));
  if (!pClientData){
    return eWLAN_PAL_STATUS_E_NOMEM;
  }

  pClientData->suspend = 0;
  WDI_DS_AssignDatapathContext(pContext, (void*)pClientData);

  pDTDriverContext = gTransportDriver.open(); 
  if( NULL == pDTDriverContext )
  {
     DTI_TRACE( DTI_TRACE_LEVEL_ERROR, " %s fail from transport open", __FUNCTION__);
     return eWLAN_PAL_STATUS_E_FAILURE;
  }
  WDT_AssignTransportDriverContext(pContext, pDTDriverContext);
  gTransportDriver.register_client(pDTDriverContext, WDTS_RxPacket, WDTS_TxPacketComplete, 
    WDTS_OOResourceNotification, (void*)pClientData);

  /* Create a memory pool for Mgmt BDheaders.*/
  sWdiStatus = WDI_DS_MemPoolCreate(&pClientData->mgmtMemPool, WDI_DS_MAX_CHUNK_SIZE, 
                                                     WDI_DS_HI_PRI_RES_NUM);
  if (WDI_STATUS_SUCCESS != sWdiStatus){
    return eWLAN_PAL_STATUS_E_NOMEM;
  }

  /* Create a memory pool for Data BDheaders.*/
  sWdiStatus = WDI_DS_MemPoolCreate(&pClientData->dataMemPool, WDI_DS_MAX_CHUNK_SIZE, 
                                                      WDI_DS_LO_PRI_RES_NUM);
  if (WDI_STATUS_SUCCESS != sWdiStatus){
    return eWLAN_PAL_STATUS_E_NOMEM;
  }

  return eWLAN_PAL_STATUS_SUCCESS;

}



/* DTS start  function. 
 * On start the transport device should start running.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along 
 * with the callback.
 *
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_startTransport( void *pContext)
{
  void *pDTDriverContext = WDT_GetTransportDriverContext(pContext);
  gTransportDriver.start(pDTDriverContext); 
  return eWLAN_PAL_STATUS_SUCCESS;

}


/* DTS Tx packet function. 
 * This function should be invoked by the DAL Dataservice to schedule transmit frame through DXE/SDIO.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along with the callback.
 * pFrame:Refernce to PAL frame.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_TxPacket(void *pContext, wpt_packet *pFrame)
{
  void *pDTDriverContext = WDT_GetTransportDriverContext(pContext);
  WDI_DS_TxMetaInfoType     *pTxMetadata;
  WDTS_ChannelType channel = WDTS_CHANNEL_TX_LOW_PRI;
  wpt_status status = eWLAN_PAL_STATUS_SUCCESS;

  // extract metadata from PAL packet
  pTxMetadata = WDI_DS_ExtractTxMetaData(pFrame);

  // assign MDPU to correct channel??
  channel =  (pTxMetadata->frmType & WDI_MAC_DATA_FRAME)? 
      WDTS_CHANNEL_TX_LOW_PRI : WDTS_CHANNEL_TX_HIGH_PRI;
  
  // Send packet to  Transport Driver. 
  status =  gTransportDriver.xmit(pDTDriverContext, pFrame, channel);
  return status;
}

/* DTS Tx Complete function. 
 * This function should be invoked by the DAL Dataservice to notify tx completion to DXE/SDIO.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along with the callback.
 * ucTxResReq:TX resource number required by TL
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_CompleteTx(void *pContext, wpt_uint32 ucTxResReq)
{
  void *pDTDriverContext = WDT_GetTransportDriverContext(pContext);
  
  // Notify completion to  Transport Driver. 
  return gTransportDriver.txComplete(pDTDriverContext, ucTxResReq);
}

/* DXE Set power state ACK callback. 
 * This callback function should be invoked by the DXE to notify WDI that set
 * power state request is complete.
 * Parameters:
 * status: status of the set operation
 * Return Value: None.
 *
 */
void  WDTS_SetPowerStateCb(wpt_status   status, unsigned int dxePhyAddr)
{
   //print a msg
   if(NULL != gSetPowerStateCbInfo.cback) 
   {
      gSetPowerStateCbInfo.cback(status, dxePhyAddr, gSetPowerStateCbInfo.pUserData);
   }
}


/* DTS Set power state function. 
 * This function should be invoked by the DAL to notify the WLAN device power state.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along with the callback.
 * powerState:Power state of the WLAN device.
 * Return Value: SUCCESS  Set successfully in DXE control blk.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_SetPowerState(void *pContext, WDTS_PowerStateType  powerState,
                              WDTS_SetPowerStateCbType cback)
{
   void *pDTDriverContext = WDT_GetTransportDriverContext(pContext);
   wpt_status status = eWLAN_PAL_STATUS_SUCCESS;

   if( cback )
   {
      //save the cback & cookie
      gSetPowerStateCbInfo.pUserData = pContext;
      gSetPowerStateCbInfo.cback = cback;
      status = gTransportDriver.setPowerState(pDTDriverContext, powerState,
                                            WDTS_SetPowerStateCb);
   }
   else
   {
      status = gTransportDriver.setPowerState(pDTDriverContext, powerState,
                                               NULL);
   }

   return status;
}

/* DTS Stop function. 
 * Stop Transport driver, ie DXE, SDIO
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along with the callback.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_Stop(void *pContext)
{
  void *pDTDriverContext = WDT_GetTransportDriverContext(pContext);
  wpt_status status = eWLAN_PAL_STATUS_SUCCESS;

  status =  gTransportDriver.stop(pDTDriverContext);

  return status;
}

/* DTS Stop function. 
 * Stop Transport driver, ie DXE, SDIO
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along with the callback.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_Close(void *pContext)
{
  void *pDTDriverContext = WDT_GetTransportDriverContext(pContext);
  WDI_DS_ClientDataType *pClientData = WDI_DS_GetDatapathContext(pContext);
  wpt_status status = eWLAN_PAL_STATUS_SUCCESS;

  /*Destroy the mem pool for mgmt BD headers*/
  WDI_DS_MemPoolDestroy(&pClientData->mgmtMemPool);
  
  /*Destroy the mem pool for mgmt BD headers*/
  WDI_DS_MemPoolDestroy(&pClientData->dataMemPool);
  
  status =  gTransportDriver.close(pDTDriverContext);

  wpalMemoryFree(pClientData);

  return status;
}

/* Get free TX data descriptor number from DXE
 * Parameters:
 * pContext: Cookie that should be passed back to the caller along with the callback.
 * Return Value: number of free descriptors for TX data channel
 *
 */
wpt_uint32 WDTS_GetFreeTxDataResNumber(void *pContext)
{
  return 
     gTransportDriver.getFreeTxDataResNumber(WDT_GetTransportDriverContext(pContext));
}
