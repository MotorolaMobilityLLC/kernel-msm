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
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file dphHashTable.h contains the definition of the scheduler class.
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#ifndef __DPH_HASH_TABLE_H__
#define __DPH_HASH_TABLE_H__

#include "aniGlobal.h"
/// Compare MAC addresses, return true if same
static inline tANI_U8
dphCompareMacAddr(tANI_U8 addr1[], tANI_U8 addr2[])
{
#if ((defined(ANI_PPC)) && defined(ANI_OS_TYPE_RTAI_LINUX))
    /*
     * Optimized comparison to take advantage of unaligned memory accesses
     * supported by the CPU.
    * This needs to be reviewed if the CPU changes.
     */

    return (((*((tANI_U32 *) addr1) - *((tANI_U32 *) addr2)) |
         (*((tANI_U16 *) &(addr1[4])) - *((tANI_U16 *) &(addr2[4])))) == 0);
#else
    return((addr1[0] == addr2[0]) &&
       (addr1[1] == addr2[1]) &&
       (addr1[2] == addr2[2]) &&
       (addr1[3] == addr2[3]) &&
       (addr1[4] == addr2[4]) &&
       (addr1[5] == addr2[5]));
#endif
}

/// Hash table class
typedef struct
{

  /// The hash table itself
  tpDphHashNode *pHashTable;

  /// The state array
  tDphHashNode  *pDphNodeArray;
  tANI_U16 size;
} dphHashTableClass;

/// The hash table object
extern dphHashTableClass dphHashTable;

/// Print MAC addresse
extern void dphPrintMacAddr(struct sAniSirGlobal *pMac, tANI_U8 addr[], tANI_U32);

tpDphHashNode dphLookupHashEntry(tpAniSirGlobal pMac, tANI_U8 staAddr[], tANI_U16 *pStaId, dphHashTableClass* pDphHashTable);
tpDphHashNode dphLookupAssocId(tpAniSirGlobal pMac,  tANI_U16 staIdx, tANI_U16* assocId, dphHashTableClass* pDphHashTable);


/// Get a pointer to the hash node
extern tpDphHashNode dphGetHashEntry(tpAniSirGlobal pMac, tANI_U16 staId, dphHashTableClass* pDphHashTable);

/// Add an entry to the hash table
extern tpDphHashNode dphAddHashEntry(tpAniSirGlobal pMac, tSirMacAddr staAddr, tANI_U16 staId, dphHashTableClass* pDphHashTable);

/// Delete an entry from the hash table
extern tSirRetStatus dphDeleteHashEntry(tpAniSirGlobal pMac, tSirMacAddr staAddr, tANI_U16 staId, dphHashTableClass* pDphHashTable);

void dphHashTableClassInit(tpAniSirGlobal pMac, dphHashTableClass* pDphHashTable);
/// Initialize STA state
extern tpDphHashNode dphInitStaState(tpAniSirGlobal pMac, tSirMacAddr staAddr,
        tANI_U16 staId, tANI_U8 validStaIdx, dphHashTableClass* pDphHashTable);


#endif
