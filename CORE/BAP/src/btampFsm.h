/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 * Copyright (c) 2008 QUALCOMM Incorporated. All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 */

/* This file is generated from btampFsm - do not edit manually*/
/* Generated on: Thu Oct 16 15:40:39 PDT 2008 / version 1.2 Beta 1 */


#ifndef __BTAMPFSM_H__
#define __BTAMPFSM_H__

#include "bapRsn8021xFsm.h"

/* State definitions */
typedef enum
{
    AUTHENTICATING,
    DISCONNECTED,
    CONNECTING,
    DISCONNECTING,
    SCANNING,
    CONNECTED,
    S1,
    KEYING,
    VALIDATED,
    STARTING,
} BTAMPFSM_STATES_T;


#if 0
/* Instance data definition of state machine */
typedef struct
{
    BTAMPFSM_ENTRY_FLAG_T disconnectedEntry;
    BTAMPFSM_STATEVAR_T stateVar;
    BTAMPFSM_INST_ID_T inst_id;
} BTAMPFSM_INSTANCEDATA_T;
#endif //0

/* Helper to initialize the machine's instance data */
#define BTAMPFSM_INSTANCEDATA_INIT {  1,  DISCONNECTED/* set init state */,  0 /* instance id */};

/*Prototype for the change state function*/
void btampfsmChangeToState(BTAMPFSM_INSTANCEDATA_T *instance, BTAMPFSM_STATES_T state);



/*Prototype of the state machine function */
//int
VOS_STATUS
btampFsm
(
    //BTAMPFSM_INSTANCEDATA_T *instanceVar
    ptBtampContext btampContext, /* btampContext value */
//    tBtampSessCtx *tpBtampSessCtx,  /* btampContext value */
    ptWLAN_BAPEvent bapEvent, /* State machine event */
    v_U8_t *status    /* return the BT-AMP status here */
);

VOS_STATUS
bapSetKey( v_PVOID_t pvosGCtx, tCsrRoamSetKey *pSetKeyInfo );

int bapSuppDisconnect(tBtampContext *ctx);
int bapAuthDisconnect(tBtampContext *ctx);
VOS_STATUS btampEstablishLogLink(ptBtampContext btampContext);
#endif
