/*
 * Copyright (c) 2011-2015 The Linux Foundation. All rights reserved.
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

/*
 *
 * This file contains the source code for CFG API functions.
 *
 * Author:      Kevin Nguyen
 * Date:        04/09/02
 * History:-
 * 04/09/02        Created.
 * --------------------------------------------------------------------
 */

#include "palTypes.h"
#include "cfgPriv.h"
#include "cfgDebug.h"
#include "wlan_qct_wda.h"

//---------------------------------------------------------------------
// Static Variables
//----------------------------------------------------------------------
static tCfgCtl   __gCfgEntry[CFG_PARAM_MAX_NUM]                ;
static tANI_U32  __gCfgIBufMin[CFG_STA_IBUF_MAX_SIZE]          ;
static tANI_U32  __gCfgIBufMax[CFG_STA_IBUF_MAX_SIZE]          ;
static tANI_U32  __gCfgIBuf[CFG_STA_IBUF_MAX_SIZE]             ;
static tANI_U8   __gCfgSBuf[CFG_STA_SBUF_MAX_SIZE]             ;
static tANI_U8   __gSBuffer[CFG_MAX_STR_LEN]                   ;

static void Notify(tpAniSirGlobal, tANI_U16, tANI_U32);


// ---------------------------------------------------------------------
tANI_U32 cfgNeedRestart(tpAniSirGlobal pMac, tANI_U16 cfgId)
{
    if (!pMac->cfg.gCfgEntry)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("gCfgEntry is NULL"));)
        return 0;
    }
    return !!(pMac->cfg.gCfgEntry[cfgId].control & CFG_CTL_RESTART) ;
}

// ---------------------------------------------------------------------
tANI_U32 cfgNeedReload(tpAniSirGlobal pMac, tANI_U16 cfgId)
{
    if (!pMac->cfg.gCfgEntry)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("gCfgEntry is NULL"));)
        return 0;
    }
    return !!(pMac->cfg.gCfgEntry[cfgId].control & CFG_CTL_RELOAD) ;
}

// ---------------------------------------------------------------------
/**
 * wlan_cfgInit()
 *
 * FUNCTION:
 * CFG initialization function.
 *
 * LOGIC:
 * Please see Configuration & Statistic Collection Micro-Architecture
 * specification for the pseudo code.
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 * This function must be called during system initialization.
 *
 * @param None
 * @return None.
 */

void
wlan_cfgInit(tpAniSirGlobal pMac)
{
    // Set status to not-ready
    pMac->cfg.gCfgStatus = CFG_INCOMPLETE;

     // Send CFG_DNLD_REQ to host
    PELOGW(cfgLog(pMac, LOGW, FL("Sending CFG_DNLD_REQ"));)
    cfgSendHostMsg(pMac, WNI_CFG_DNLD_REQ, WNI_CFG_DNLD_REQ_LEN,
                   WNI_CFG_DNLD_REQ_NUM, 0, 0, 0);

} /*** end wlan_cfgInit() ***/

void cfg_get_str_index(tpAniSirGlobal mac_ctx, uint16_t cfg_id)
{
    uint16_t i = 0;

    for (i = 0; i < CFG_MAX_STATIC_STRING; i++) {
        if (cfg_id == cfg_static_string[i].cfg_id)
            break;
    }

    if (i == CFG_MAX_STATIC_STRING) {
        PELOGE(cfgLog(mac_ctx, LOGE,
               FL("Entry not found for cfg id :%d"), cfg_id);)
        cfg_static[cfg_id].p_str_data = NULL;
        return;
    }

    cfg_static[cfg_id].p_str_data = &cfg_static_string[i];
}


//---------------------------------------------------------------------
tSirRetStatus cfgInit(tpAniSirGlobal pMac)
{
    uint16_t i = 0;
    pMac->cfg.gCfgIBufMin  = __gCfgIBufMin;
    pMac->cfg.gCfgIBufMax  = __gCfgIBufMax;
    pMac->cfg.gCfgIBuf     = __gCfgIBuf;
    pMac->cfg.gCfgSBuf     = __gCfgSBuf;
    pMac->cfg.gSBuffer     = __gSBuffer;
    pMac->cfg.gCfgEntry    = __gCfgEntry;

    for (i = 0; i < CFG_PARAM_MAX_NUM; i++) {
        if (!(cfg_static[i].control & CFG_CTL_INT))
            cfg_get_str_index(pMac, i);
        else
            cfg_static[i].p_str_data = NULL;
    }

    return (eSIR_SUCCESS);
}

//----------------------------------------------------------------------
void cfgDeInit(tpAniSirGlobal pMac)
{
   pMac->cfg.gCfgIBufMin  = NULL;
   pMac->cfg.gCfgIBufMax  = NULL;
   pMac->cfg.gCfgIBuf     = NULL;
   pMac->cfg.gCfgSBuf     = NULL;
   pMac->cfg.gSBuffer     = NULL;
   pMac->cfg.gCfgEntry    = NULL;
}

// ---------------------------------------------------------------------
/**
 * cfgSetInt()
 *
 * FUNCTION:
 * This function is called to update an integer parameter.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * - Range checking is performed by the calling function.  In case this
 *   function call is being triggered by a request from host, then host
 *   is responsible for performing range checking before sending the
 *   request.
 *
 * - Host RW permission checking should already be done prior to calling
 *   this function by the message processing function.
 *
 * NOTE:
 *
 * @param cfgId:     16-bit CFG parameter ID
 * @param value:     32-bit unsigned value
 *
 * @return eSIR_SUCCESS       :  request completed successfully \n
 * @return eSIR_CFG_INVALID_ID:  invalid CFG parameter ID \n
 */

tSirRetStatus
cfgSetInt(tpAniSirGlobal pMac, tANI_U16 cfgId, tANI_U32 value)
{
    tANI_U32      index;
    tANI_U32      control, mask;
    tSirRetStatus  retVal;

    if (cfgId >= CFG_PARAM_MAX_NUM)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("Invalid cfg id %d"), cfgId);)
        return eSIR_CFG_INVALID_ID;
    }
    if (!pMac->cfg.gCfgEntry)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("gCfgEntry is NULL"));)
        return eSIR_CFG_INVALID_ID;
    }

    control  = pMac->cfg.gCfgEntry[cfgId].control;
    index    = control & CFG_BUF_INDX_MASK;
    retVal   = eSIR_SUCCESS;

    if (index >= CFG_STA_IBUF_MAX_SIZE)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("cfg index out of bounds %d"), index);)
        retVal = eSIR_CFG_INVALID_ID;
        return retVal;
    }

    // Check if parameter is valid
    if ((control & CFG_CTL_VALID) == 0)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("Not valid cfg id %d"), cfgId);)
        retVal = eSIR_CFG_INVALID_ID;
    }
    else if ((pMac->cfg.gCfgIBufMin[index] > value) ||
             (pMac->cfg.gCfgIBufMax[index] < value))
    {
        PELOGE(cfgLog(pMac, LOGE, FL("Value %d out of range [%d,%d] cfg id %d"),
               value, pMac->cfg.gCfgIBufMin[index],
               pMac->cfg.gCfgIBufMax[index], cfgId);)
        retVal = eSIR_CFG_INVALID_ID;
    }
    else
    {
            // Write integer value
            pMac->cfg.gCfgIBuf[index] = value;

            // Update hardware if necessary
            mask = control & CFG_CTL_NTF_MASK;
            if ((mask & CFG_CTL_NTF_HW) != 0)
                PELOGE(cfgLog(pMac, LOGE, FL("CFG Notify HW not supported!!!"));)

            // Notify other modules if necessary
            if ((mask & CFG_CTL_NTF_MASK) != 0)
                Notify(pMac, cfgId, mask);

    }

    return (retVal);

} /*** end cfgSetInt ***/

// ---------------------------------------------------------------------
/**
 * cfgCheckValid()
 *
 * FUNCTION:
 * This function is called to check if a parameter is valid
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param cfgId:  16-bit CFG parameter ID
 *
 * @return eSIR_SUCCESS:         request completed successfully
 * @return eSIR_CFG_INVALID_ID:  invalid CFG parameter ID
 */

tSirRetStatus
cfgCheckValid(tpAniSirGlobal pMac, tANI_U16 cfgId)
{
    tANI_U32      control;

    if (cfgId >= CFG_PARAM_MAX_NUM)
    {
        PELOG3(cfgLog(pMac, LOG3, FL("Invalid cfg id %d"), cfgId);)
        return(eSIR_CFG_INVALID_ID);
    }
    if (!pMac->cfg.gCfgEntry)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("gCfgEntry is NULL"));)
        return eSIR_CFG_INVALID_ID;
    }

    control = pMac->cfg.gCfgEntry[cfgId].control;

    // Check if parameter is valid
    if ((control & CFG_CTL_VALID) == 0)
    {
        PELOG3(cfgLog(pMac, LOG3, FL("Not valid cfg id %d"), cfgId);)
        return(eSIR_CFG_INVALID_ID);
    }
    else
        return(eSIR_SUCCESS);

} /*** end cfgCheckValid() ***/

// ---------------------------------------------------------------------
/**
 * wlan_cfgGetInt()
 *
 * FUNCTION:
 * This function is called to read an integer parameter.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param cfgId:  16-bit CFG parameter ID
 * @param pVal:   address where parameter value will be written
 *
 * @return eSIR_SUCCESS:         request completed successfully
 * @return eSIR_CFG_INVALID_ID:  invalid CFG parameter ID
 */

tSirRetStatus
wlan_cfgGetInt(tpAniSirGlobal pMac, tANI_U16 cfgId, tANI_U32 *pValue)
{
    tANI_U32      index;
    tANI_U32      control;
    tSirRetStatus  retVal;

    if (cfgId >= CFG_PARAM_MAX_NUM)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("Invalid cfg id %d"), cfgId);)
        retVal = eSIR_CFG_INVALID_ID;
        return retVal;
    }
    if (!pMac->cfg.gCfgEntry)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("gCfgEntry is NULL"));)
        return eSIR_CFG_INVALID_ID;
    }

    control = pMac->cfg.gCfgEntry[cfgId].control;
    index   = control & CFG_BUF_INDX_MASK;
    retVal  = eSIR_SUCCESS;

    if (index >= CFG_STA_IBUF_MAX_SIZE)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("cfg index out of bounds %d"), index);)
        retVal = eSIR_CFG_INVALID_ID;
        return retVal;
    }

    // Check if parameter is valid
    if ((control & CFG_CTL_VALID) == 0)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("Not valid cfg id %d"), cfgId);)
        retVal = eSIR_CFG_INVALID_ID;
    }
    else {
        // Get integer value
        if (index < CFG_STA_IBUF_MAX_SIZE)
            *pValue = pMac->cfg.gCfgIBuf[index];
    }

    return (retVal);

} /*** end wlan_cfgGetInt() ***/

// ---------------------------------------------------------------------
/**
 * cfgSetStr()
 *
 * FUNCTION:
 * This function is called to set a string parameter.
 *
 * LOGIC:
 * This function invokes the cfgSetStrNotify function passing the notify
 * boolean value set to TRUE. This basically means that HAL needs to be
 * notified. This is true in the case of non-integrated SOC's or Libra/Volans.
 * In the case of Prima the cfgSetStrNotify is invoked with the boolean value
 * set to FALSE.
 *
 * ASSUMPTIONS:
 * - always Notify has to be called
 *
 * NOTE:
 *
 * @param cfgId:     16-bit CFG parameter ID
 * @param pStr:      address of string data
 * @param len:       string length
 *
 * @return eSIR_SUCCESS:         request completed successfully
 * @return eSIR_CFG_INVALID_ID:  invalid CFG parameter ID
 * @return eSIR_CFG_INVALID_LEN: invalid parameter length
 *
 */

tSirRetStatus cfgSetStr(tpAniSirGlobal pMac, tANI_U16 cfgId, tANI_U8 *pStr,
                                          tANI_U32 length)
{
   return cfgSetStrNotify( pMac, cfgId, pStr, length, TRUE );
}

// ---------------------------------------------------------------------
/**
 * cfgSetStrNotify()
 *
 * FUNCTION:
 * This function is called to set a string parameter.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * - No length checking will be performed.  Should be done by calling
 *   module.
 * - Host RW permission should be checked prior to calling this
 *   function.
 *
 * NOTE:
 *
 * @param cfgId:     16-bit CFG parameter ID
 * @param pStr:      address of string data
 * @param len:       string length
 * @param notifyMod. Notify respective Module
 *
 * @return eSIR_SUCCESS:         request completed successfully
 * @return eSIR_CFG_INVALID_ID:  invalid CFG parameter ID
 * @return eSIR_CFG_INVALID_LEN: invalid parameter length
 *
 */

tSirRetStatus
cfgSetStrNotify(tpAniSirGlobal pMac, tANI_U16 cfgId, tANI_U8 *pStr,
                                          tANI_U32 length, int notifyMod)
{
    tANI_U8       *pDst, *pDstEnd;
    tANI_U32      index, paramLen, control, mask;
    tSirRetStatus  retVal;

    if (cfgId >= CFG_PARAM_MAX_NUM)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("Invalid cfg id %d"), cfgId);)
        return eSIR_CFG_INVALID_ID;
    }
    if (!pMac->cfg.gCfgEntry)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("gCfgEntry is NULL"));)
        return eSIR_CFG_INVALID_ID;
    }

    control  = pMac->cfg.gCfgEntry[cfgId].control;
    index    = control & CFG_BUF_INDX_MASK;
    retVal   = eSIR_SUCCESS;

    // Check if parameter is valid
    if ((control & CFG_CTL_VALID) == 0)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("Invalid cfg id %d"), cfgId);)
        retVal = eSIR_CFG_INVALID_ID;
    }
    else if (index >= CFG_STA_SBUF_MAX_SIZE)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("Invalid Sbuf index %d (max size %d)"),
               index, CFG_STA_SBUF_MAX_SIZE);)
        retVal = eSIR_CFG_INVALID_ID;
    }
    else
    {
            pDst = &pMac->cfg.gCfgSBuf[index];
            paramLen = *pDst++;
            if (length > paramLen)
            {
                PELOGE(cfgLog(pMac, LOGE, FL("Invalid length %d (>%d) cfg id %d"),
                       length, paramLen, cfgId);)
                retVal = eSIR_CFG_INVALID_LEN;
            }
            else
            {
                *pDst++ = (tANI_U8)length;
                pDstEnd = pDst + length;
                while (pDst < pDstEnd)
                {
                    *pDst++ = *pStr++;
                }

                if(notifyMod)
                {
                    // Update hardware if necessary
                    mask = control & CFG_CTL_NTF_MASK;
                    if ((mask & CFG_CTL_NTF_HW) != 0)
                    {
                        PELOGE(cfgLog(pMac, LOGE, FL("CFG Notify HW not supported!!!"));)
                    }

                    // Notify other modules if necessary
                    if ( (mask & CFG_CTL_NTF_MASK) != 0)
                    {
                        Notify(pMac, cfgId, mask);
                    }
                }
            }

        }

    return (retVal);

} /*** end cfgSetStrNotify() ***/

// ---------------------------------------------------------------------
/**
 * wlan_cfgGetStr()
 *
 * FUNCTION:
 * This function is called to get a string parameter.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * - Host RW permission should be checked prior to calling this
 *   function.
 *
 * NOTE:
 *
 * @param cfgId:     16-bit CFG parameter ID
 * @param pBuf:      address of string buffer
 * @param pLen:      address of max buffer length
 *                   actual length will be returned at this address
 *
 * @return eSIR_SUCCESS:         request completed successfully
 * @return eSIR_CFG_INVALID_ID:  invalid CFG parameter ID
 * @return eSIR_CFG_INVALID_LEN: invalid parameter length
 *
 */

tSirRetStatus
wlan_cfgGetStr(tpAniSirGlobal pMac, tANI_U16 cfgId, tANI_U8 *pBuf, tANI_U32 *pLength)
{
    tANI_U8             *pSrc, *pSrcEnd;
    tANI_U32            index, control;
    tSirRetStatus  retVal;

    if (cfgId >= CFG_PARAM_MAX_NUM)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("Invalid cfg id %d"), cfgId);)
        retVal = eSIR_CFG_INVALID_ID;
        return retVal;
    }
    if (!pMac->cfg.gCfgEntry)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("gCfgEntry is NULL"));)
        return eSIR_CFG_INVALID_ID;
    }

    control  = pMac->cfg.gCfgEntry[cfgId].control;
    index    = control & CFG_BUF_INDX_MASK;
    retVal   = eSIR_SUCCESS;

    if (index >= CFG_STA_SBUF_MAX_SIZE)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("cfg index out of bounds %d"), index);)
        retVal = eSIR_CFG_INVALID_ID;
        return retVal;
    }

    // Check if parameter is valid
    if ((control & CFG_CTL_VALID) == 0)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("Not valid cfg id %d"), cfgId);)
        retVal = eSIR_CFG_INVALID_ID;
    }
    else
    {
        // Get string
        pSrc  = &pMac->cfg.gCfgSBuf[index];
        pSrc++;                               // skip over max length
        if (*pLength < *pSrc)
        {
            PELOGE(cfgLog(pMac, LOGE, FL("Invalid length %d (<%d) cfg id %d"),
                   *pLength, *pSrc, cfgId);)
            retVal = eSIR_CFG_INVALID_LEN;
        }
        else
        {
            *pLength = *pSrc++;               // save parameter length
            pSrcEnd = pSrc + *pLength;
            while (pSrc < pSrcEnd)
                *pBuf++ = *pSrc++;
        }
    }

    return (retVal);

} /*** end wlan_cfgGetStr() ***/

// ---------------------------------------------------------------------
/**
 * wlan_cfgGetStrMaxLen()
 *
 * FUNCTION:
 * This function is called to get a string maximum length.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * - Host RW permission should be checked prior to calling this
 *   function.
 *
 * NOTE:
 *
 * @param cfgId:     16-bit CFG parameter ID
 * @param pLen:      maximum length will be returned at this address
 *
 * @return eSIR_SUCCESS:         request completed successfully
 * @return eSIR_CFG_INVALID_ID:  invalid CFG parameter ID
 *
 */

tSirRetStatus
wlan_cfgGetStrMaxLen(tpAniSirGlobal pMac, tANI_U16 cfgId, tANI_U32 *pLength)
{
    tANI_U32            index, control;
    tSirRetStatus  retVal;

    if (cfgId >= CFG_PARAM_MAX_NUM)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("Invalid cfg id %d"), cfgId);)
        retVal = eSIR_CFG_INVALID_ID;
    }
    if (!pMac->cfg.gCfgEntry)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("gCfgEntry is NULL"));)
        return eSIR_CFG_INVALID_ID;
    }

    control  = pMac->cfg.gCfgEntry[cfgId].control;
    index    = control & CFG_BUF_INDX_MASK;
    retVal   = eSIR_SUCCESS;

    if (index >= CFG_STA_SBUF_MAX_SIZE)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("cfg index out of bounds %d"), index);)
        retVal = eSIR_CFG_INVALID_ID;
        return retVal;
    }

    // Check if parameter is valid
    if ((control & CFG_CTL_VALID) == 0)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("Not valid cfg id %d"), cfgId);)
        retVal = eSIR_CFG_INVALID_ID;
    }
    else
    {
        *pLength = pMac->cfg.gCfgSBuf[index];
    }

    return (retVal);

} /*** end wlan_cfgGetStrMaxLen() ***/

// ---------------------------------------------------------------------
/**
 * wlan_cfgGetStrLen()
 *
 * FUNCTION:
 * This function is called to get a string length.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * - Host RW permission should be checked prior to calling this
 *   function.
 *
 * NOTE:
 *
 * @param cfgId:     16-bit CFG parameter ID
 * @param pLen:      current length will be returned at this address
 *
 * @return eSIR_SUCCESS:         request completed successfully
 * @return eSIR_CFG_INVALID_ID:  invalid CFG parameter ID
 *
 */

tSirRetStatus
wlan_cfgGetStrLen(tpAniSirGlobal pMac, tANI_U16 cfgId, tANI_U32 *pLength)
{
    tANI_U32            index, control;
    tSirRetStatus  retVal;

    if (cfgId >= CFG_PARAM_MAX_NUM)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("Invalid cfg id %d"), cfgId);)
        retVal = eSIR_CFG_INVALID_ID;
    }
    if (!pMac->cfg.gCfgEntry)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("gCfgEntry is NULL"));)
        return eSIR_CFG_INVALID_ID;
    }

    control  = pMac->cfg.gCfgEntry[cfgId].control;
    index    = control & CFG_BUF_INDX_MASK;
    retVal   = eSIR_SUCCESS;

    if (index >= CFG_STA_SBUF_MAX_SIZE-1)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("cfg index out of bounds %d"), index);)
        retVal = eSIR_CFG_INVALID_ID;
        return retVal;
    }

    // Check if parameter is valid
    if ((control & CFG_CTL_VALID) == 0)
    {
        PELOGE(cfgLog(pMac, LOGE, FL("Not valid cfg id %d"), cfgId);)
        retVal = eSIR_CFG_INVALID_ID;
    }
    else
    {
        *pLength = pMac->cfg.gCfgSBuf[index+1];
    }

    return (retVal);

} /*** end wlan_cfgGetStrLen() ***/



/*-------------------------------------------------------------
\fn     cfgGetDot11dTransmitPower
\brief  This function returns the regulatory max transmit power
\param  pMac
\return tPowerdBm - Power
\-------------------------------------------------------------*/
static tPowerdBm
cfgGetDot11dTransmitPower(tpAniSirGlobal pMac, tANI_U16   cfgId,
                                        tANI_U32 cfgLength, tANI_U8 channel)
{
    tANI_U8    *pCountryInfo = NULL;
    tANI_U8    count = 0;
    tPowerdBm  maxTxPwr = WDA_MAX_TXPOWER_INVALID;

    /* At least one element is present */
    if(cfgLength < sizeof(tSirMacChanInfo))
    {
        PELOGE(cfgLog(pMac, LOGE, FL("Invalid CFGLENGTH %d while getting 11d txpower"), cfgLength);)
        goto error;
    }

    pCountryInfo = vos_mem_malloc(cfgLength);
    if ( NULL == pCountryInfo )
    {
        cfgLog(pMac, LOGP, FL(" failed to allocate memory"));
        goto error;
    }
    /* The CSR will always update this CFG. The contents will be from country IE if regulatory domain
     * is enabled on AP else will contain EEPROM contents
     */
    if (wlan_cfgGetStr(pMac, cfgId, pCountryInfo, &cfgLength) != eSIR_SUCCESS)
    {
        vos_mem_free(pCountryInfo);
        pCountryInfo = NULL;

        cfgLog(pMac, LOGP, FL("Failed to retrieve 11d configuration parameters while retrieving 11d tuples"));
        goto error;
    }
    /* Identify the channel and max txpower */
    while(count <= (cfgLength - (sizeof(tSirMacChanInfo))))
    {
        tANI_U8    firstChannel, maxChannels;

        firstChannel = pCountryInfo[count++];
        maxChannels = pCountryInfo[count++];
        maxTxPwr = pCountryInfo[count++];

        if((channel >= firstChannel) &&
            (channel < (firstChannel + maxChannels)))
        {
            break;
        }
    }

error:
    if (NULL != pCountryInfo)
        vos_mem_free(pCountryInfo);

    return maxTxPwr;
}


/**----------------------------------------------------------------------
\fn     cfgGetRegulatoryMaxTransmitPower

\brief  Gets regulatory tx power on the current channel.

\param  pMac
\param  channel
\param  rfBand
 -----------------------------------------------------------------------*/
tPowerdBm cfgGetRegulatoryMaxTransmitPower(tpAniSirGlobal pMac, tANI_U8 channel)
{
    tANI_U32    cfgLength = 0;
    tANI_U16    cfgId = 0;
    tPowerdBm  maxTxPwr;
    eRfBandMode rfBand = eRF_BAND_UNKNOWN;

    if ((channel >= SIR_11A_CHANNEL_BEGIN) &&
        (channel <= SIR_11A_CHANNEL_END))
        rfBand = eRF_BAND_5_GHZ;
    else
        rfBand = eRF_BAND_2_4_GHZ;


    /* Get the max transmit power for current channel for the current regulatory domain */
    switch (rfBand)
    {
        case eRF_BAND_2_4_GHZ:
            cfgId = WNI_CFG_MAX_TX_POWER_2_4;
            cfgLength = WNI_CFG_MAX_TX_POWER_2_4_LEN;
            PELOG2(cfgLog(pMac, LOG2, FL("HAL: Reading CFG for 2.4 GHz channels to get regulatory max tx power"));)
            break;

        case eRF_BAND_5_GHZ:
            cfgId = WNI_CFG_MAX_TX_POWER_5;
            cfgLength = WNI_CFG_MAX_TX_POWER_5_LEN;
            PELOG2(cfgLog(pMac, LOG2, FL("HAL: Reading CFG for 5.0 GHz channels to get regulatory max tx power"));)
            break;

        case eRF_BAND_UNKNOWN:
        default:
            PELOG2(cfgLog(pMac, LOG2, FL("HAL: Invalid current working band for the device"));)
            return WDA_MAX_TXPOWER_INVALID; //Its return, not break.
    }

    maxTxPwr = cfgGetDot11dTransmitPower(pMac, cfgId, cfgLength, channel);

    return (maxTxPwr);
}

// ---------------------------------------------------------------------
/**
 * cfgGetCapabilityInfo
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param None
 * @return None
 */

tSirRetStatus
cfgGetCapabilityInfo(tpAniSirGlobal pMac, tANI_U16 *pCap,tpPESession sessionEntry)
{
    tANI_U32 val = 0;
    tpSirMacCapabilityInfo pCapInfo;

    *pCap = 0;
    pCapInfo = (tpSirMacCapabilityInfo) pCap;

    if (LIM_IS_IBSS_ROLE(sessionEntry))
        pCapInfo->ibss = 1; // IBSS bit
    else if (LIM_IS_AP_ROLE(sessionEntry) ||
             LIM_IS_BT_AMP_AP_ROLE(sessionEntry) ||
             LIM_IS_BT_AMP_STA_ROLE(sessionEntry) ||
             LIM_IS_STA_ROLE(sessionEntry))
        pCapInfo->ess = 1; // ESS bit
    else if (LIM_IS_P2P_DEVICE_ROLE(sessionEntry) ||
             LIM_IS_NDI_ROLE(sessionEntry)) {
        pCapInfo->ess = 0;
        pCapInfo->ibss = 0;
    }
    else
        cfgLog(pMac, LOGP, FL("can't get capability, role is UNKNOWN!!"));


    if (LIM_IS_AP_ROLE(sessionEntry)) {
        val = sessionEntry->privacy;
#ifdef SAP_AUTH_OFFLOAD
         /* Support software AP Authentication Offload feature,
          * If Auth offload security Type is not disabled
          * We need to enable privacy bit in beacon
          */
        if (pMac->sap_auth_offload && pMac->sap_auth_offload_sec_type) {
            val = 1;
        }
#endif
    } else {
        // PRIVACY bit
        if (wlan_cfgGetInt(pMac, WNI_CFG_PRIVACY_ENABLED, &val) != eSIR_SUCCESS)
        {
            cfgLog(pMac, LOGP, FL("cfg get WNI_CFG_PRIVACY_ENABLED failed"));
            return eSIR_FAILURE;
        }
    }
    if (val)
        pCapInfo->privacy = 1;

    // Short preamble bit
    if (wlan_cfgGetInt(pMac, WNI_CFG_SHORT_PREAMBLE, &val) != eSIR_SUCCESS)
    {
        cfgLog(pMac, LOGP, FL("cfg get WNI_CFG_SHORT_PREAMBLE failed"));
        return eSIR_FAILURE;
    }
    if (val)
        pCapInfo->shortPreamble = 1;


    // PBCC bit
    pCapInfo->pbcc = 0;

    // Channel agility bit
    pCapInfo->channelAgility = 0;
    //If STA/AP operating in 11B mode, don't set rest of the capability info bits.
    if(sessionEntry->dot11mode == WNI_CFG_DOT11_MODE_11B)
        return eSIR_SUCCESS;

    // Short slot time bit
    if (LIM_IS_AP_ROLE(sessionEntry)) {
        pCapInfo->shortSlotTime = sessionEntry->shortSlotTimeSupported;
    } else {
        if (wlan_cfgGetInt(pMac, WNI_CFG_11G_SHORT_SLOT_TIME_ENABLED, &val)
                       != eSIR_SUCCESS)
        {
            cfgLog(pMac, LOGP,
                   FL("cfg get WNI_CFG_11G_SHORT_SLOT_TIME failed"));
            return eSIR_FAILURE;
        }
        /* When in STA mode, we need to check if short slot is enabled as well as check if the current operating
         * mode is short slot time and then decide whether to enable short slot or not. It is safe to check both
         * cfg values to determine short slot value in this funcn since this funcn is always used after assoc when
         * these cfg values are already set based on peer's capability. Even in case of IBSS, its value is set to
         * correct value either in delBSS as part of deleting the previous IBSS or in start BSS as part of coalescing
         */
        if (val)
        {
            pCapInfo->shortSlotTime = sessionEntry->shortSlotTimeSupported;
        }
    }

    // Spectrum Management bit
    if (!LIM_IS_IBSS_ROLE(sessionEntry) && sessionEntry->lim11hEnable) {
      if (wlan_cfgGetInt(pMac, WNI_CFG_11H_ENABLED, &val) != eSIR_SUCCESS)
      {
          cfgLog(pMac, LOGP, FL("cfg get WNI_CFG_11H_ENABLED failed"));
          return eSIR_FAILURE;
      }
      if (val)
          pCapInfo->spectrumMgt = 1;
    }

    // QoS bit
    if (wlan_cfgGetInt(pMac, WNI_CFG_QOS_ENABLED, &val) != eSIR_SUCCESS)
    {
        cfgLog(pMac, LOGP, FL("cfg get WNI_CFG_QOS_ENABLED failed"));
        return eSIR_FAILURE;
    }
    if (val)
        pCapInfo->qos = 1;

    // APSD bit
    if (wlan_cfgGetInt(pMac, WNI_CFG_APSD_ENABLED, &val) != eSIR_SUCCESS)
    {
        cfgLog(pMac, LOGP, FL("cfg get WNI_CFG_APSD_ENABLED failed"));
        return eSIR_FAILURE;
    }
    if (val)
        pCapInfo->apsd = 1;

#if defined WLAN_FEATURE_VOWIFI
    pCapInfo->rrm = pMac->rrm.rrmSmeContext.rrmConfig.rrm_enabled;
#if defined WLAN_VOWIFI_DEBUG
    cfgLog( pMac, LOG1, "RRM = %d", pCapInfo->rrm);
#endif
#endif

    //DSSS-OFDM
    //FIXME : no config defined yet.

    // Block ack bit
    if (wlan_cfgGetInt(pMac, WNI_CFG_BLOCK_ACK_ENABLED, &val) != eSIR_SUCCESS)
    {
        cfgLog(pMac, LOGP, FL("cfg get WNI_CFG_BLOCK_ACK_ENABLED failed"));
        return eSIR_FAILURE;
    }
    pCapInfo->delayedBA = (tANI_U16)((val >> WNI_CFG_BLOCK_ACK_ENABLED_DELAYED) & 1);
    pCapInfo->immediateBA = (tANI_U16)((val >> WNI_CFG_BLOCK_ACK_ENABLED_IMMEDIATE) & 1);

    return eSIR_SUCCESS;
}

// --------------------------------------------------------------------
/**
 * cfgSetCapabilityInfo
 *
 * FUNCTION:
 * This function is called on BP based on the capabilities
 * received in SME_JOIN/REASSOC_REQ message.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE: 1. ESS/IBSS capabilities are based on system role.
 *       2. Since PBCC, Channel agility and Extended capabilities
 *          are not supported, they're not set at CFG
 *
 * @param  pMac   Pointer to global MAC structure
 * @param  caps   16-bit Capability Info field
 * @return None
 */

void
cfgSetCapabilityInfo(tpAniSirGlobal pMac, tANI_U16 caps)
{
}


// ---------------------------------------------------------------------
/**
 * cfgCleanup()
 *
 * FUNCTION:
 * CFG clean up function.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 * This function must be called during system shut down.
 *
 * @param None
 *
 * @return None.
 *
 */

void
cfgCleanup(tpAniSirGlobal pMac)
{
    // Set status to not-ready
    pMac->cfg.gCfgStatus = CFG_INCOMPLETE;

} /*** end CfgCleanup() ***/

// ---------------------------------------------------------------------
/**
 * Notify()
 *
 * FUNCTION:
 * This function is called to notify other modules of parameter update.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param cfgId:    configuration parameter ID
 * @param mask:     notification mask
 *
 * @return None.
 *
 */

static void
Notify(tpAniSirGlobal pMac, tANI_U16 cfgId, tANI_U32 ntfMask)
{

    tSirMsgQ    mmhMsg;

    mmhMsg.type = SIR_CFG_PARAM_UPDATE_IND;
    mmhMsg.bodyval = (tANI_U32)cfgId;
    mmhMsg.bodyptr = NULL;

    MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));

    if ((ntfMask & CFG_CTL_NTF_SCH) != 0)
        schPostMessage(pMac, &mmhMsg);

    if ((ntfMask & CFG_CTL_NTF_LIM) != 0)
        limPostMsgApi(pMac, &mmhMsg);

    if ((ntfMask & CFG_CTL_NTF_HAL) != 0)
        wdaPostCtrlMsg(pMac, &mmhMsg);

    // Notify ARQ

} /*** end Notify() ***/

/**
 * cfg_get_vendor_ie_ptr_from_oui() - returns IE pointer in IE buffer given its
 * OUI and OUI size
 * @mac_ctx:    mac context.
 * @oui:        OUI string.
 * @oui_size:   length of OUI string
 *              One can provide multiple line descriptions
 *              for arguments.
 * @ie:         ie buffer
 * @ie_len:     length of ie buffer
 *
 * This function parses the IE buffer and finds the given OUI and returns its
 * pointer
 *
 * Return: pointer of given OUI IE else NULL
 */
uint8_t* cfg_get_vendor_ie_ptr_from_oui(tpAniSirGlobal mac_ctx,
					uint8_t *oui,
					uint8_t oui_size,
					uint8_t *ie,
					uint16_t ie_len)
{
	int32_t left = ie_len;
	uint8_t *ptr = ie;
	uint8_t elem_id, elem_len;

	while(left >= 2) {
		elem_id  = ptr[0];
		elem_len = ptr[1];
		left -= 2;
		if(elem_len > left) {
			cfgLog(mac_ctx, LOGE,
			FL("Invalid IEs eid = %d elem_len=%d left=%d"),
			elem_id, elem_len, left);
			return NULL;
		}
		if (SIR_MAC_EID_VENDOR == elem_id) {
			if(memcmp(&ptr[2], oui, oui_size)==0)
			return ptr;
		}

		left -= elem_len;
		ptr += (elem_len + 2);
	}
	return NULL;
}
// ---------------------------------------------------------------------
