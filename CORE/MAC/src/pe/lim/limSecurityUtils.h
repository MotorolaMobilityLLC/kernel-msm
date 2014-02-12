/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file limSecurityUtils.h contains the utility definitions
 * related to WEP encryption/decryption etc.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */
#ifndef __LIM_SECURITY_UTILS_H
#define __LIM_SECURITY_UTILS_H
#include "sirMacProtDef.h" //for tSirMacAuthFrameBody

#define LIM_ENCR_AUTH_BODY_LEN  (sizeof(tSirMacAuthFrameBody) + \
                                SIR_MAC_WEP_IV_LENGTH + \
                                SIR_MAC_WEP_ICV_LENGTH)
struct tLimPreAuthNode;

tANI_U8        limIsAuthAlgoSupported(tpAniSirGlobal, tAniAuthType, tpPESession);

// MAC based authentication related functions
void               limInitPreAuthList(tpAniSirGlobal);
void               limDeletePreAuthList(tpAniSirGlobal);
struct tLimPreAuthNode    *limSearchPreAuthList(tpAniSirGlobal, tSirMacAddr);
void               limAddPreAuthNode(tpAniSirGlobal, struct tLimPreAuthNode *);
void               limDeletePreAuthNode(tpAniSirGlobal, tSirMacAddr);
void               limReleasePreAuthNode(tpAniSirGlobal pMac, tpLimPreAuthNode pAuthNode);
void               limRestoreFromAuthState(tpAniSirGlobal,
                                           tSirResultCodes, tANI_U16,tpPESession);

// Encryption/Decryption related functions
tCfgWepKeyEntry    *limLookUpKeyMappings(tSirMacAddr);
void               limComputeCrc32(tANI_U8 *, tANI_U8 *, tANI_U8);
void               limRC4(tANI_U8 *, tANI_U8 *, tANI_U8 *, tANI_U32, tANI_U16);
void               limEncryptAuthFrame(tpAniSirGlobal, tANI_U8, tANI_U8 *, tANI_U8 *, tANI_U8 *, tANI_U32);
tANI_U8                 limDecryptAuthFrame(tpAniSirGlobal, tANI_U8 *, tANI_U8 *, tANI_U8 *, tANI_U32, tANI_U16);

void limSendSetBssKeyReq( tpAniSirGlobal, tLimMlmSetKeysReq *,tpPESession );
void limSendSetStaKeyReq( tpAniSirGlobal, tLimMlmSetKeysReq *, tANI_U16, tANI_U8,tpPESession);
void limPostSmeSetKeysCnf( tpAniSirGlobal, tLimMlmSetKeysReq *, tLimMlmSetKeysCnf * );

void limSendRemoveBssKeyReq(tpAniSirGlobal pMac, tLimMlmRemoveKeyReq * pMlmRemoveKeyReq,tpPESession);
void limSendRemoveStaKeyReq(tpAniSirGlobal pMac, tLimMlmRemoveKeyReq * pMlmRemoveKeyReq, tANI_U16 staIdx,tpPESession);
void limPostSmeRemoveKeyCnf(tpAniSirGlobal pMac, tpPESession psessionEntry, tLimMlmRemoveKeyReq * pMlmRemoveKeyReq, tLimMlmRemoveKeyCnf * mlmRemoveKeyCnf);

#define  PTAPS  0xedb88320

static inline tANI_U32
limCrcUpdate(tANI_U32 crc, tANI_U8 x)
{

    // Update CRC computation for 8 bits contained in x
    //
    tANI_U32 z;
    tANI_U32 fb;
    int i;

    z = crc^x;
    for (i=0; i<8; i++) {
        fb = z & 1;
        z >>= 1;
        if (fb) z ^= PTAPS;
    }
    return z;
}

#endif /* __LIM_SECURITY_UTILS_H */
