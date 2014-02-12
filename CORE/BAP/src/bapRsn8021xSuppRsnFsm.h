/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 * Woodside Networks, Inc proprietary. All rights reserved.
 * $File: //depot/software/projects/feature_branches/gen5_phase1/os/linux/classic/ap/apps/ssm/auth8021x/ani8021xSuppRsnFsm.h $
 *
 * Contains declarations for the RSN EAPOL-Key FSM on the
 * supplicant side. This is based on 802.11i.
 *
 * Author:      Mayank D. Upadhyay
 * Date:        19-December-2002
 * History:-
 * Date         Modified by     Modification Information
 * ------------------------------------------------------
 *
 */
#ifndef __AAG_SUPP_RSN_FSM_H
#define __AAG_SUPP_RSN_FSM_H

#include "bapRsn8021xFsm.h"
#include "bapInternal.h"

/**
 * suppRsnFsmInit
 *
 * FUNCTION:
 * Initializes the constants and the callbacks needed by this FSM
 * module.
 *
 * @param cb callbacks to the various procedures needed by this FSM
 *
 * @return ANI_OK if the operation succeeds
 */
int
suppRsnFsmInit(void);

int
suppRsnFsmSetPmk(tSuppContext *ctx, v_U8_t *pmk);

#endif //__AAG_SUPP_RSN_FSM_H
