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

/*
 *
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * Author:              Kevin Nguyen
 * Date:                02/27/02
 * History:-
 * 02/12/02             Created.
 * --------------------------------------------------------------------
 *
 */

#ifndef __UTILSAPI_H
#define __UTILSAPI_H

#include <stdarg.h>
#include <sirCommon.h>
#include "aniGlobal.h"
#include "utilsGlobal.h"
#if defined VOSS_ENABLED
#include "VossWrapper.h"
#elif defined ANI_OS_TYPE_LINUX
#include "rtaiWrapper.h"
#elif defined ANI_OS_TYPE_OSX
#include "palApiPci.h"
#endif


#if defined ANI_OS_TYPE_LINUX

extern void rt_sched_lock();
extern void rt_sched_unlock();

#define SIR_DECLARE_FLAGS   tANI_U32 flags
#define SIR_SCHED_LOCK()    rt_sched_lock()
#define SIR_SCHED_UNLOCK()  rt_sched_unlock()

#else

#define SIR_DECLARE_FLAGS
#define SIR_SCHED_LOCK()    TX_DISABLE_INTR;
#define SIR_SCHED_UNLOCK()  TX_ENABLE_INTR;

#endif

#define LOG_INDEX_FOR_MODULE( modId ) ( ( modId ) - LOG_FIRST_MODULE_ID )
#define GET_MIN_VALUE(__val1, __val2) ((__val1 < __val2) ? __val1 : __val2)

// The caller must check loglevel. This API assumes loglevel is good
extern void logDebug(tpAniSirGlobal pMac, tANI_U8 modId, tANI_U32 debugLevel, const char *pStr, va_list marker);

extern void logDbg(tpAniSirGlobal pMac, tANI_U8 modId, tANI_U32 debugLevel, const char *pStr,...);

extern tANI_U32 gPktAllocCnt, gPktFreeCnt;

/// Debug dumps
extern void logPrintf(tpAniSirGlobal, tANI_U32, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4);

/// RTAI dump
extern int logRtaiDump(tpAniSirGlobal, tANI_U32, tANI_U32, tANI_U32, tANI_U32, tANI_U32, tANI_U8 *);

/// Log initialization
extern tSirRetStatus logInit (tpAniSirGlobal);

extern void
logDeinit(tpAniSirGlobal );

extern tSirRetStatus cfgInit(tpAniSirGlobal);
extern void cfgDeInit(tpAniSirGlobal);

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

void sirDumpBuf(tpAniSirGlobal pMac, tANI_U8 modId, tANI_U32 level, tANI_U8 *buf, tANI_U32 size);

#if defined ANI_OS_TYPE_LINUX


    struct rtLibApp;
    void* rtaiBufAlloc(struct rtLibApp * rt, tANI_U16 size, tANI_U32 waitOpt);
    tSirRetStatus rtaiBufInit(unsigned int radioId,  t_mac_block_table* block_table);
    tANI_U16 rtaiBufAvail(tANI_U16 size);

extern void sysSuspendThreads(tpAniSirGlobal pMac);

#define sharedBufAlloc(pMac, x, y) rtaiBufAlloc(pMac->rt,x,y)
#define bufInit(x) rtaiBufInit(x)
#define bufAvail(x) rtaiBufAvail(x)

#define BUF_32                  32
#define BUF_64                  64
#define BUF_96                  96
#define BUF_128                 128
#define BUF_160                 160
#define BUF_256                 256
#define BUF_512                 512
#define BUF_1024                1024
#define BUF_1536                1536
#define BUF_2048                2048
#define BUF_8192                8192
#define RTAI_MAX_BUF_SIZE       BUF_8192

#endif

// --------------------------------------------------------------------
/**
 * sirSwapU16()
 *
 * FUNCTION:
 * This function is called to swap two U8s of an tANI_U16 value
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  val    tANI_U16 value to be tANI_U8 swapped
 * @return        Swapped tANI_U16 value
 */

static inline tANI_U16
sirSwapU16(tANI_U16 val)
{
    return(((val & 0x00FF) << 8) | ((val & 0xFF00) >> 8));
}/*** end sirSwapU16() ***/

// --------------------------------------------------------------------
/**
 * sirSwapU16ifNeeded()
 *
 * FUNCTION:
 * This function is called to swap two U8s of an tANI_U16 value depending
 * on endiannes of the target processor/compiler the software is
 * running on
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  val    tANI_U16 value to be tANI_U8 swapped
 * @return        Swapped tANI_U16 value
 */

static inline tANI_U16
sirSwapU16ifNeeded(tANI_U16 val)
{
#ifndef ANI_LITTLE_BYTE_ENDIAN
    return sirSwapU16(val);
#else
    return val;
#endif
}/*** end sirSwapU16ifNeeded() ***/

// --------------------------------------------------------------------
/**
 * sirSwapU32()
 *
 * FUNCTION:
 * This function is called to swap four U8s of an tANI_U32 value
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  val    tANI_U32 value to be tANI_U8 swapped
 * @return        Swapped tANI_U32 value
 */

static inline tANI_U32
sirSwapU32(tANI_U32 val)
{
    return((val << 24) |
           (val >> 24) |
           ((val & 0x0000FF00) << 8) |
           ((val & 0x00FF0000) >> 8));
}/*** end sirSwapU32() ***/

// --------------------------------------------------------------------
/**
 * sirSwapU32ifNeeded()
 *
 * FUNCTION:
 * This function is called to swap U8s of an tANI_U32 value depending
 * on endiannes of the target processor/compiler the software is
 * running on
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  val    tANI_U32 value to be tANI_U8 swapped
 * @return        Swapped tANI_U32 value
 */

static inline tANI_U32
sirSwapU32ifNeeded(tANI_U32 val)
{
#ifndef ANI_LITTLE_BYTE_ENDIAN
    return sirSwapU32(val);
#else
    return val;
#endif
}/*** end sirSwapU32ifNeeded() ***/




// -------------------------------------------------------------------
/**
 * sirSwapU32Buf
 *
 * FUNCTION:
 * It swaps N dwords into the same buffer
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  ptr address of tANI_U32 array
 * @return void
 *
 */

static inline void
sirSwapU32Buf(tANI_U32 *ptr, tANI_U32 nWords)
{
    tANI_U32     i;

    for (i=0; i < nWords; i++)
        ptr[i] = sirSwapU32(ptr[i]);
}

// --------------------------------------------------------------------
/**
 * sirSwapU32BufIfNeeded()
 *
 * FUNCTION:
 * This function is called to swap U8s of U32s in the buffer depending
 * on endiannes of the target processor/compiler the software is
 * running on
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  pBuf   Buffer that will get swapped
 * @param  nWords Number DWORDS will be swapped
 * @return        void
 */

static inline void
sirSwapU32BufIfNeeded(tANI_U32* pBuf, tANI_U32 nWords)
{
#ifdef ANI_LITTLE_BYTE_ENDIAN
    sirSwapU32Buf(pBuf, nWords);
#endif
}/*** end sirSwapU32ifNeeded() ***/


// --------------------------------------------------------------------
/**
 * sirSwapBDIfNeeded
 *
 * FUNCTION:
 * Byte swap all the dwords in the BD, except the PHY/MAC headers
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  pBd    BD that will get swapped
 * @return        void
 */

static inline void
sirSwapBDIfNeeded(tANI_U32 *pBd)
{
    sirSwapU32BufIfNeeded(pBd, 6);
    sirSwapU32BufIfNeeded(pBd+18, 14);
}


// -------------------------------------------------------------------
/**
 * sirStoreU16N
 *
 * FUNCTION:
 * It stores a 16 bit number into the byte array in network byte order
 * i.e. the least significant byte first
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  ptr address of destination byte array
 * @param  val value to store
 * @return None
 */

static inline void
sirStoreU16N(tANI_U8 *ptr, tANI_U16 val)
{
    *ptr++ = (val >> 8) & 0xff;
    *ptr = val & 0xff;
}

// -------------------------------------------------------------------
/**
 * sirStoreU32N
 *
 * FUNCTION:
 * It stores a 32 bit number into the byte array in network byte order
 * i.e. the least significant byte first
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  ptr address of destination byte array
 * @param  val value to store
 * @return None
 */

static inline void
sirStoreU32N(tANI_U8 *ptr, tANI_U32 val)
{
    *ptr++ = (tANI_U8) (val >> 24) & 0xff;
    *ptr++ = (tANI_U8) (val >> 16) & 0xff;
    *ptr++ = (tANI_U8) (val >> 8) & 0xff;
    *ptr = (tANI_U8) (val) & 0xff;
}

// -------------------------------------------------------------------
/**
 * sirStoreU16
 *
 * FUNCTION:
 * It stores a 16 bit number into the byte array in NON-network byte order
 * i.e. the least significant byte first
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  ptr address of destination byte array
 * @param  val value to store
 * @return None
 */

static inline void
sirStoreU16(tANI_U8 *ptr, tANI_U16 val)
{
    *ptr++ = val & 0xff;
    *ptr = (val >> 8) & 0xff;
}

// -------------------------------------------------------------------
/**
 * sirStoreU32
 *
 * FUNCTION:
 * It stores a 32 bit number into the byte array in NON-network byte order
 * i.e. the least significant byte first
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  ptr address of destination byte array
 * @param  val value to store
 * @return None
 */

static inline void
sirStoreU32(tANI_U8 *ptr, tANI_U32 val)
{
    *ptr++ = (tANI_U8) val & 0xff;
    *ptr++ = (tANI_U8) (val >> 8) & 0xff;
    *ptr++ = (tANI_U8) (val >> 16) & 0xff;
    *ptr = (tANI_U8) (val >> 24) & 0xff;
}

// -------------------------------------------------------------------
/**
 * sirStoreU32BufN
 *
 * FUNCTION:
 * It stores a 32 bit number into the byte array in network byte order
 * i.e. the least significant byte first. It performs the above operation
 * on entire buffer and writes to the dst buffer
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * Assumes that the pSrc buffer is of all tANI_U32 data type fields.
 *
 * NOTE:
 * Must be used if all the fields in the buffer must be of tANI_U32 types.
 *
 * @param  pDst   address of destination byte array
 * @param  pSrc   address of the source DWORD array
 * @param  length number of DWORDs
 * @return None
 */

static inline void
sirStoreBufN(tANI_U8* pDst, tANI_U32* pSrc, tANI_U32 length)
{
    while (length)
    {
        sirStoreU32N(pDst, *pSrc);
        pDst += 4;
        pSrc++;
        length--;
    }
}

// -------------------------------------------------------------------
/**
 * sirReadU16N
 *
 * FUNCTION:
 * It reads a 16 bit number from the byte array in network byte order
 * i.e. the least significant byte first
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  ptr address of  byte array
 * @return 16 bit value
 */

static inline tANI_U16
sirReadU16N(tANI_U8 *ptr)
{
    return(((*ptr) << 8) |
           (*(ptr+1)));
}
/**
 * sirSwapU32Buf
 *
 * FUNCTION:
 * It swaps N dwords into the same buffer
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  ptr address of tANI_U32 array
 * @return void
 *
 */

static inline void
sirSwapNStore(tANI_U32 *src, tANI_U32 *dst, tANI_U32 nWords)
{
    tANI_U32     i;

    for (i=0; i < nWords; i++)
        dst[i] = sirSwapU32(src[i]);
}

// -------------------------------------------------------------------
/**
 * sirReadU32N
 *
 * FUNCTION:
 * It reads a 32 bit number from the byte array in network byte order
 * i.e. the least significant byte first
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  ptr address of  byte array
 * @return 32 bit value
 */

static inline tANI_U32
sirReadU32N(tANI_U8 *ptr)
{
    return((*(ptr) << 24) |
           (*(ptr+1) << 16) |
           (*(ptr+2) << 8) |
           (*(ptr+3)));
}

// -------------------------------------------------------------------
/**
 * sirReadU16
 *
 * FUNCTION:
 * It reads a 16 bit number from the byte array in NON-network byte order
 * i.e. the least significant byte first
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  ptr address of  byte array
 * @return 16 bit value
 */

static inline tANI_U16
sirReadU16(tANI_U8 *ptr)
{
    return((*ptr) |
           (*(ptr+1) << 8));
}

// -------------------------------------------------------------------
/**
 * sirReadU32
 *
 * FUNCTION:
 * It reads a 32 bit number from the byte array in NON-network byte order
 * i.e. the least significant byte first
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  ptr address of  byte array
 * @return 32 bit value
 */

static inline tANI_U32
sirReadU32(tANI_U8 *ptr)
{
    return((*(ptr)) |
           (*(ptr+1) << 8) |
           (*(ptr+2) << 16) |
           (*(ptr+3) << 24));
}

// -------------------------------------------------------------------


/// Copy a MAC address from 'from' to 'to'
static inline void
sirCopyMacAddr(tANI_U8 to[], tANI_U8 from[])
{
#if defined( _X86_ )
    tANI_U32 align = (0x3 & ((tANI_U32) to | (tANI_U32) from ));
    if( align ==0){
       *((tANI_U16 *) &(to[4])) = *((tANI_U16 *) &(from[4]));
       *((tANI_U32 *) to) = *((tANI_U32 *) from);
    }else if (align == 2){
        *((tANI_U16 *) &to[4]) = *((tANI_U16 *) &from[4]);
        *((tANI_U16 *) &to[2]) = *((tANI_U16 *) &from[2]);
        *((tANI_U16 *) &to[0]) = *((tANI_U16 *) &from[0]);
    }else{
       to[5] = from[5];
       to[4] = from[4];
       to[3] = from[3];
       to[2] = from[2];
       to[1] = from[1];
       to[0] = from[0];
    }
#else
       to[0] = from[0];
       to[1] = from[1];
       to[2] = from[2];
       to[3] = from[3];
       to[4] = from[4];
       to[5] = from[5];
#endif
}

static inline tANI_U8
sirCompareMacAddr(tANI_U8 addr1[], tANI_U8 addr2[])
{
#if defined( _X86_ )
    tANI_U32 align = (0x3 & ((tANI_U32) addr1 | (tANI_U32) addr2 ));

    if( align ==0){
        return ((*((tANI_U16 *) &(addr1[4])) == *((tANI_U16 *) &(addr2[4])))&&
                (*((tANI_U32 *) addr1) == *((tANI_U32 *) addr2)));
    }else if(align == 2){
        return ((*((tANI_U16 *) &addr1[4]) == *((tANI_U16 *) &addr2[4])) &&
            (*((tANI_U16 *) &addr1[2]) == *((tANI_U16 *) &addr2[2])) &&
            (*((tANI_U16 *) &addr1[0]) == *((tANI_U16 *) &addr2[0])));
    }else{
        return ( (addr1[5]==addr2[5])&&
            (addr1[4]==addr2[4])&&
            (addr1[3]==addr2[3])&&
            (addr1[2]==addr2[2])&&
            (addr1[1]==addr2[1])&&
            (addr1[0]==addr2[0]));
    }
#else
         return ( (addr1[0]==addr2[0])&&
            (addr1[1]==addr2[1])&&
            (addr1[2]==addr2[2])&&
            (addr1[3]==addr2[3])&&
            (addr1[4]==addr2[4])&&
            (addr1[5]==addr2[5]));
#endif
}


/*
* converts tANI_U16 CW value to 4 bit value to be inserted in IE
*/
static inline tANI_U8 convertCW(tANI_U16 cw)
{
    tANI_U8 val = 0;
    while (cw > 0)
        {
            val++;
            cw >>= 1;
        }
    if (val > 15)
        return 0xF;
    return val;
}

/* The user priority to AC mapping is such:
 *   UP(1, 2) ---> AC_BK(1)
 *   UP(0, 3) ---> AC_BE(0)
 *   UP(4, 5) ---> AC_VI(2)
 *   UP(6, 7) ---> AC_VO(3)
 */
#define WLAN_UP_TO_AC_MAP            0x33220110
#define upToAc(up)                ((WLAN_UP_TO_AC_MAP >> ((up) << 2)) & 0x03)

#if defined VOSS_ENABLED

#define sirBusyWait(microsecond)   vos_busy_wait(microsecond / 1000)
#define sirSleepWait(duration)  vos_sleep_us(duration)

#elif defined ANI_OS_TYPE_LINUX
    //rt_busy_sleep(duration)

#define sirBusyWait(duration)   tx_busy_wait(duration)

#define sirSleepWait(duration)  sirSleepWaitIntern(duration)

#elif defined ANI_OS_TYPE_WINDOWS

#define sirBusyWait(duration)   sirBusyWaitIntern(pMac, duration)

#define sirSleepWait(duration)  sirSleepWaitIntern(pMac, duration)

#elif defined ANI_OS_TYPE_OSX
#define sirBusyWait(duration) palBusyWait(duration)
#define sirSleepWait(duration) palSleepWait(duration)
#endif



/**---------------------------------------------------------------------
 * sirSleepWait
 *
 * FUNCTION:
 * This function is called to yield the CPU for a given duration
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  duration    Duration to yield (nanoseconds)
 * @return None
 */

#if defined ANI_OS_TYPE_WINDOWS

static inline void
sirSleepWaitIntern(void *pMac, tANI_U32 duration)

#else

static inline void
sirSleepWaitIntern(tANI_U32 duration)

#endif
{
#if defined ANI_OS_TYPE_LINUX
    //temporary measure: not sure we can sleep less than 500usec on rtai
    //so sleep at this amount of time so as we are garanteed to yield
    if (duration<500000)
        duration=500000;
    tx_thread_sleep(duration);
    //    rt_sleep(duration/40/*temporary magic number: clock is 25MHz*/);
#elif defined ANI_OS_TYPE_WINDOWS
    // Can't sleep on windows at dispatch level
    // what to do here?
    if (duration >= 50000)
    {
        tANI_U32     i;

        for (i = duration / 50000; i; i--)
            sirBusyWait(50000);
    }
#endif
} // sirSleepWait

// -------------------------------------------------------------------

/// Parse the next IE in a message
extern tSirRetStatus sirParseNextIE(tpAniSirGlobal, tANI_U8 *pPayload,
                                     tANI_U16 payloadLength, tANI_S16 lastType,
                                     tANI_U8 *pType, tANI_U8 *pLength);

/// Check if the given channel is 11b channel
#define SIR_IS_CHANNEL_11B(chId)  (chId <= 14)

// -------------------------------------------------------------------
/**
 * halRoundS32
 *
 * FUNCTION:
 * Performs integer rounding like returns 12346 for 123456 or -12346 for -123456
 * Note that a decimal place is lost.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  tANI_S32 input
 * @return rounded number
 */
static inline tANI_S32
halRoundS32(tANI_S32 p)
{
    tANI_S32  k, i, j;

    i = p/10;
    j = p%10;
    if (p > 0)
        k = i + (j > 4 ? 1 : 0);
    else if (p < 0)
        k = i + (j < -5 ? -1 : 0);
    else
        k = p;

        return(k);
}

// New functions for endianess conversion
#ifdef ANI_LITTLE_BYTE_ENDIAN
#define ani_cpu_to_be16(x) sirSwapU16((x))
#define ani_cpu_to_le16(x) (x)
#define ani_cpu_to_be32(x) sirSwapU32((x))
#define ani_cpu_to_le32(x) (x)
#else // ANI_LITTLE_BYTE_ENDIAN
#define ani_cpu_to_be16(x) (x)
#define ani_cpu_to_le16(x) sirSwapU16((x))
#define ani_cpu_to_be32(x) (x)
#define ani_cpu_to_le32(x) sirSwapU32((x))
#endif // ANI_LITTLE_BYTE_ENDIAN

#define ani_le16_to_cpu(x)  ani_cpu_to_le16(x)
#define ani_le32_to_cpu(x)  ani_cpu_to_le32(x)
#define ani_be16_to_cpu(x)  ani_cpu_to_be16(x)
#define ani_be32_to_cpu(x)  ani_cpu_to_be32(x)

void ConverttoBigEndian(void *ptr, tANI_U16 size);
void CreateScanCtsFrame(tpAniSirGlobal pMac, tSirMacMgmtHdr *macMgmtHdr, tSirMacAddr selfMac);
void CreateScanDataNullFrame(tpAniSirGlobal pMac, tSirMacMgmtHdr *macMgmtHdr,
                             tANI_U8 pwrMgmt, tSirMacAddr bssid, 
                             tSirMacAddr selfMacAddr);
void CreateInitScanRawFrame(tpAniSirGlobal pMac, tSirMacMgmtHdr *macMgmtHdr, tBssSystemRole role);
void CreateFinishScanRawFrame(tpAniSirGlobal pMac, tSirMacMgmtHdr *macMgmtHdr, tBssSystemRole role);

#endif /* __UTILSAPI_H */


