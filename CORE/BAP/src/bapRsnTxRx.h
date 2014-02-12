/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined( __BAPRSN_TXRX_H )
#define __BAPRSN_TXRX_H

/**=============================================================================
  
  bapRsnTxRx.h
  
  \brief
  
  Description...
    
  
               Copyright 2008 (c) Qualcomm, Incorporated.
               All Rights Reserved.
               Qualcomm Confidential and Proprietary.
  
  ==============================================================================*/

#include "vos_types.h"
#include "vos_status.h"
#include "vos_packet.h"
#include "bapRsnAsfPacket.h"


typedef int (*pnfTxCompleteHandler)( v_PVOID_t pvosGCtx, vos_pkt_t *pPacket, VOS_STATUS retStatus );
typedef int (*pnfRxFrameHandler)( v_PVOID_t pvosGCtx, vos_pkt_t *pPacket );

/*
    \brief bapRsnSendEapolFrame
    To push an eapol frame to TL. 

    \param pAniPkt - a ready eapol frame that is prepared in tAniPacket format
*/
VOS_STATUS bapRsnSendEapolFrame( v_PVOID_t pvosGCtx, tAniPacket *pAniPkt );


/*
    \brief bapRsnRegisterTxRxCallbacks
    To register two callbacks for txcomplete and rxFrames .

    \param pfnTxCom - pointer to a function to handle the tx completion.
    \param pnfRxFrame - point to a function to handle rx frames
*/
VOS_STATUS bapRsnRegisterTxRxCallbacks( pnfTxCompleteHandler pfnTxCom, pnfRxFrameHandler pnfRxFrame );

//To set the callbaks to NULL so it can be change later
void bapRsnClearTxRxCallbacks(void);

/*
    \brief bapRsnRegisterRxCallback
    To register the RX frame callbacks to TL to receive EAPOL frames .

    \param pvosGCtx - pointer to global VOSS context.
*/
VOS_STATUS bapRsnRegisterRxCallback( v_PVOID_t pvosGCtx );

VOS_STATUS bapRsnRxCallback(v_PVOID_t pv, vos_pkt_t *pPacket);

#endif //__BAPRSN_TXRX_H




