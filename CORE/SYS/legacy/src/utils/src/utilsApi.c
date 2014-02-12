/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
//==================================================================
//
//  File:         utilsApi.cc
//
//  Description:  Implemention of a few utility routines. 
//
//  Author:       Neelay Das
//
//  Copyright 2003, Woodside Networks, Inc. All rights reserved.
//
//  Change gHistory:
//  12/15/2003 - NDA - Initial version.
//
//===================================================================


#include "utilsApi.h"





// -------------------------------------------------------------------
/**
 * sirDumpBuf()
 *
 * FUNCTION:
 * This function is called to dump a buffer with a certain level
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param pBuf: buffer pointer
 * @return None.
 */
void
sirDumpBuf(tpAniSirGlobal pMac, tANI_U8 modId, tANI_U32 level, tANI_U8 *buf, tANI_U32 size)
{
    tANI_U32 i;

    if (level > pMac->utils.gLogDbgLevel[LOG_INDEX_FOR_MODULE(modId)])
        return;

    logDbg(pMac, modId, level, FL("Dumping %d bytes in host order\n"), size);

    for (i=0; (i+7)<size; i+=8)
    {
        logDbg(pMac, modId, level,
                 "%02x %02x %02x %02x %02x %02x %02x %02x \n",
                 buf[i],
                 buf[i+1],
                 buf[i+2],
                 buf[i+3],
                 buf[i+4],
                 buf[i+5],
                 buf[i+6],
                 buf[i+7]);
    }

    // Dump the bytes in the last line
    for (; i < size; i++)
    {
        logDbg(pMac, modId, level, "%02x ", buf[i]);

        if((i+1) == size)
            logDbg(pMac, modId, level, "\n");
    }

}/*** end sirDumpBuf() ***/
