/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 * Woodside Networks, Inc proprietary. All rights reserved.
 * $File: //depot/software/projects/feature_branches/gen5_phase1/os/linux/classic/ap/apps/ssm/auth8021x/ani8021xAuthRsnFsm.h $
 *
 * Contains declarations for the RSN EAPOL-Key FSM on the
 * authenticator side. This is based on 802.11i.
 *
 * Author:      Mayank D. Upadhyay
 * Date:        19-December-2002
 * History:-
 * Date         Modified by     Modification Information
 * ------------------------------------------------------
 *
 */
#ifndef __AAG_AUTH_RSN_FSM_H
#define __AAG_AUTH_RSN_FSM_H

#include "vos_types.h"
#include "vos_timer.h"

#include "bapInternal.h"
#include "bapRsn8021xFsm.h"



/**
 * authRsnFsmInit
 *
 * FUNCTION:
 * Initializes the constants and the callbacks needed by this FSM
 * module.
 *
 * @param consts the various constant values needed by this FSM
 * @param cb callbacks to the various procedures needed by this FSM
 *
 * @return ANI_OK if the operation succeeds
 */
int
authRsnFsmInit(tAuthRsnFsmConsts *consts);

/**
 * authRsnFsmCreate
 *
 * FUNCTION
 * Allocates and initializes the state of an RSN key FSM instance for
 * the given STA context.
 * 
 * @parm ctx the STA context whose instance is being created
 * @param pskBased pass in eANI_BOOLEAN_TRUE is this STA is to be
 * authenticated based on a pre-shared key as opposed to EAP.
 *
 * @return ANI_OK if the operation succeeds
 */
int
authRsnFsmCreate(tBtampContext *ctx);



int
authRsnFsmSetPmk(tStaContext *ctx, v_U8_t *pmk);

#endif //__AAG_AUTH_RSN_FSM_H
