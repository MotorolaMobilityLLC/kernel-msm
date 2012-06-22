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
  
  \file  limSessionUtils.c
  
  \brief implementation for lim Session Utility  APIs

  \author Sunit Bhatia
  
  Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
  ========================================================================*/


/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "aniGlobal.h"
#include "limDebug.h"
#include "limSession.h"
#include "limSessionUtils.h"
#include "limUtils.h"


/*--------------------------------------------------------------------------
  \brief peValidateJoinReq() - validates the Join request .

  This function is called to validate the Join Request for a BT-AMP station. If start BSS session is present
  this function returns TRUE else returns FALSE.
  PE will force SME to first issue ''START_BSS' request for BTAMP_STA, before sending a JOIN request.
    
  \param pMac                   - pointer to global adapter context
  \return                            - return TRUE if start BSS session is present else return FALSE.
  
  \sa
  --------------------------------------------------------------------------*/

tANI_U8 peValidateBtJoinRequest(tpAniSirGlobal pMac)
{

    tANI_U8 i;
    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if( (pMac->lim.gpSession[i].valid) && 
            (pMac->lim.gpSession[i].bssType == eSIR_BTAMP_STA_MODE) &&
            (pMac->lim.gpSession[i].statypeForBss == STA_ENTRY_SELF))
        {
            return(TRUE); 
        }

    }
    return(FALSE);

}

/*--------------------------------------------------------------------------
  \brief peGetValidPowerSaveSession() - Fetches the valid session for powersave .

  This function is called to check the valid session for power save, if more than one session is active , this function 
  it returns NULL.
  if there is only one valid "infrastructure" session present in "linkestablished" state this function returns sessionentry.
  For all other cases it returns NULL.
    
  \param pMac                   - pointer to global adapter context
  \return                            - return session is address if valid session is  present else return NULL.
  
  \sa
  --------------------------------------------------------------------------*/


tpPESession peGetValidPowerSaveSession(tpAniSirGlobal pMac)
{
    tANI_U8 i;
    tANI_U8 sessioncount = 0;
    tANI_U8 sessionId = 0;

    for(i = 0; i < pMac->lim.maxBssId; i++)
    {
        if( (pMac->lim.gpSession[i].valid == TRUE)&&
            (pMac->lim.gpSession[i].limSystemRole == eLIM_STA_ROLE)&&
            (pMac->lim.gpSession[i].limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE)) {
            sessioncount++;
            sessionId = i;

            if(sessioncount > 1)
            {
                return(NULL);
            }
        }

    }

    if( (pMac->lim.gpSession[sessionId].valid == TRUE)&&
        (pMac->lim.gpSession[sessionId].limSystemRole == eLIM_STA_ROLE)&&
        (pMac->lim.gpSession[sessionId].limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE))

    {
       return(&pMac->lim.gpSession[sessionId]);
    }
    return(NULL);
    
}
/*--------------------------------------------------------------------------
  \brief peIsAnySessionActive() - checks for the active session presence .

  This function returns TRUE if atleast one valid session is present else it returns FALSE
      
  \param pMac                   - pointer to global adapter context
  \return                            - return TRUE if atleast one session is active else return FALSE.
  
  \sa
  --------------------------------------------------------------------------*/


tANI_U8 peIsAnySessionActive(tpAniSirGlobal pMac)
{
    tANI_U8 i;
    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if(pMac->lim.gpSession[i].valid == TRUE) 
        {
            return(TRUE);
        }

    }
    return(FALSE);

}

/*--------------------------------------------------------------------------
  \brief isLimSessionOffChannel() - Determines if the there is any other off channel 
                                    session.

  This function returns TRUE if the session Id passed needs to be on a different
  channel than atleast one session already active.
    
  \param pMac                   - pointer to global adapter context
  \param sessionId              - session ID of the session to be verified.  
  
  \return tANI_U8               - Boolean value for off-channel operation.
  
  \sa
  --------------------------------------------------------------------------*/

tANI_U8
isLimSessionOffChannel(tpAniSirGlobal pMac, tANI_U8 sessionId)
{
    tANI_U8 i;

    if(sessionId >=  pMac->lim.maxBssId)
    {
        limLog(pMac, LOGE, FL("Invalid sessionId: %d \n "), sessionId);
        return FALSE;
    }

    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if( i == sessionId )
        {
          //Skip the sessionId that is to be joined.
          continue;
        }
        //if snother ession is valid and it is on different channel
        //it is an off channel operation.
        if( (pMac->lim.gpSession[i].valid) && 
            (pMac->lim.gpSession[i].currentOperChannel != 
             pMac->lim.gpSession[sessionId].currentOperChannel) )
        {
            return TRUE;
        }
    }

    return FALSE;

}

tANI_U8
peGetActiveSessionChannel (tpAniSirGlobal pMac)
{
    tANI_U8 i;

    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        //if snother ession is valid and it is on different channel
        //it is an off channel operation.
        if(pMac->lim.gpSession[i].valid)
        {
            return pMac->lim.gpSession[i].currentOperChannel;
        }
    }

    return 0;

}

