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

/** ------------------------------------------------------------------------- *
    ------------------------------------------------------------------------- *


    \file palPipes.h

    \brief Defines Pal pipes to transfer frames/memory to/from the device

    $Id$


    Copyright (C) 2006 Airgo Networks, Incorporated

   ========================================================================== */

#ifndef PALPIPES_H
#define PALPIPES_H

#include "halTypes.h"


typedef enum
{
    CSR_PIPE =  0,  //CSR Endpoint for USB and non-DXE transfer for PCI/PCIe

    PIPE_TX_1 = 1,  //lowest priority
    PIPE_TX_2 = 2,
    PIPE_TX_3 = 3,
    PIPE_TX_4 = 4,
    PIPE_TX_5 = 5,
    PIPE_TX_6 = 6,
    PIPE_TX_7 = 7,

    PIPE_RX_1 = 8,  //lowest priority
    PIPE_RX_2 = 9,
    PIPE_RX_3 = 10,
    PIPE_RX_4 = 11,
    PIPE_RX_5 = 12,
    PIPE_RX_6 = 13,
    PIPE_RX_7 = 14,

    MAX_PIPE = PIPE_RX_7,
    ANY_PIPE
}ePipes;


/* sHddMemSegment defines the structure that the HDD will use to transfer frames
    down through the hal to the pal layer.
*/
typedef struct tagHddMemSegment
{
    struct tagHddMemSegment *next;      //linked list, NULL=no more segments linked
    void *hddContext;                   //provides a place for the HDD layer to record information
    void *addr;                         //either virtual or physical memory address. PCI will likely be physical addresses, USB will be virtual addresses
                                        //pal layer will convert virtual memory addresses to physical for transfer with DXE.
    tANI_U32 length;                    //number of bytes contained in segment for transfer.
}sHddMemSegment;



/* sTxFrameTransfer defines the structure that Hal's transmit queueing will use to
    keep frames in queues, ready for transfer to the pal layer.
*/
typedef struct tagTxFrameTransfer
{
    struct tagTxFrameTransfer *next;   //used by hal to queue transmit frames, Pal does nothing with this
    sHddMemSegment *bd;                //if NULL, the BD should be created during DXE transfer to asic
    sHddMemSegment *segment;           //all segments associated with an inidividual frame
    tANI_U32 descCount;                 //number of descriptors needed to transfer
}sTxFrameTransfer;




#define PAL_RX_BUFFER_SIZE  (16 * 1024)
typedef tANI_U8 *tRxBuffer;

typedef struct
{
    tANI_U32 nBytes;                    //number of bytes transferred to the buffer
    tRxBuffer pBbuffer;                 //rxBuffer virtual pointer
    tANI_BUFFER_ADDR physAddr;               //rxBuffer physical address
                                        //it is assumed that the BD is present and we know how to interpret the whole buffer
}sRxFrame;



/* sRxFrameTransfer defines the structure that Hal's receive queuing will use to
    keep frames in receive queues.
*/
typedef struct tagRxFrameTransfer
{
    struct tagRxFrameTransfer *next;    //used by hal to queue transmit frames, Pal does nothing with this
    ePipes pipe;                        //pipe the frame was received on - need to free the correct resources quickly
    sRxFrame *rxFrame;                  //virtual memory pointer to raw frame data
}sRxFrameTransfer;



typedef union
{
    sTxFrameTransfer txFrame;
    sRxFrameTransfer rxFrame;
}uFrameTransfer;


typedef eHalStatus (*palXfrCompleteFPtr)(tHalHandle hHal, ePipes pipe, uFrameTransfer *frame);


typedef struct
{
    tANI_U32 nDescs;                    //number of URBs for USB or descriptors for DXE that can be queued for transfer at one time
    tANI_U32 nRxBuffers;                //maximum number of receive buffers from physical memory to use for this pipe
    tANI_BOOLEAN preferCircular;        //1 = use circular descriptor handling if available, linear otherwise
    tANI_BOOLEAN bdPresent;             //1 = BD attached to frames for this pipe
    palXfrCompleteFPtr callback;        //callback when transfer is complete, if not NULL
    palXfrCompleteFPtr rxCallback;      //Rx callback when transfer type is H2H, if not NULL
    tANI_U32 refWQ;             // Reference WQ - for H2B and B2H only
    tANI_U32 xfrType;           // H2B(Tx), B2H(Rx), H2H(SRAM<->HostMem R/W)
    tANI_U32 chPriority;        // Channel Priority 7(Highest) - 0(Lowest)
    tANI_BOOLEAN bCfged;        //whether the pipe has been configured
    tANI_U32 indexEP;     //This is for USB only, it is the index of TX/RX endpoint, TX_DATA_PIPE_OUT...
    tANI_U32 bmuThreshold; //BMU threshold 
    // For PAL's internal use
    void *pReserved1;
    void *pReserved2;

    tANI_BOOLEAN use_lower_4g;  /**< Added for Gen5 Prefetch */
    tANI_BOOLEAN use_short_desc_fmt;
}sPipeCfg;




#define PIPE_MEM_XFR_LIMIT  (16 * 1024)   //maximum single transfer size




// palPipes Functions....
#define palIsPipeCfg(hHdd, pipeNum) ((hHdd)->PipeCfgInfo[(pipeNum)].bCfged)
eHalStatus palPipeCfg(tHddHandle, ePipes pipe, sPipeCfg *pPipeCfg);
eHalStatus palWriteFrame(tHddHandle, ePipes pipe, sTxFrameTransfer *frame);
eHalStatus palFreeRxFrame(tHddHandle, sRxFrameTransfer *frame);

#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "halInterrupts.h"
eHalStatus palTxPipeIsr(tHalHandle hHal, eHalIntSources intSource);
eHalStatus palRxPipeIsr(tHalHandle hHal, eHalIntSources intSource);
#endif

#endif
